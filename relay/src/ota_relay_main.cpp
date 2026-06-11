#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <csignal>
#include "CommonAPI/CommonAPI.hpp"
#include <shared/command_types.hpp>
#include "relay/download_client.hpp"
#include "relay/daemon_controller.hpp"
#include "relay/relay_control_impl.hpp"
#include <nlohmann/json.hpp>
#include <openssl/md5.h>

using json = nlohmann::json;
using namespace std::chrono_literals;

static std::atomic<bool> g_running{true};

static void signalHandler(int) {
    g_running = false;
}

static std::string computeFileMd5(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::vector<char> buf(8192);
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5_CTX ctx;
    MD5_Init(&ctx);
    while (f.read(buf.data(), buf.size()) || f.gcount())
        MD5_Update(&ctx, buf.data(), f.gcount());
    MD5_Final(digest, &ctx);
    char hex[33];
    for (int i = 0; i < 16; i++)
        snprintf(hex + i * 2, 3, "%02x", digest[i]);
    return std::string(hex);
}

class OtaRelay {
public:
    OtaRelay(const RelayConfig& config)
        : config_(config)
        , running_(false)
        , currentVersionId_(0)
        , downloadedVersionId_(0)
        , pendingVersionId_(0)
        , currentState_(relay::RelayState::IDLE)
        , downloadClient_(config.serviceDomain, config.serviceInstance)
        , daemonController_(config.daemonDomain, config.daemonInstance) {}

    bool init();
    void run();
    void stop() { g_running = false; running_ = false; }
    bool loadConfigFromFile(const std::string& configPath);

private:
    RelayConfig config_;
    std::atomic<bool> running_;
    std::shared_ptr<CommonAPI::Runtime> runtime_;

    uint32_t currentVersionId_;
    std::string currentVersionString_;
    uint32_t downloadedVersionId_;
    uint32_t pendingVersionId_;
    std::string pendingFilePath_;

    relay::RelayState currentState_;
    std::mutex stateMutex_;

    std::chrono::steady_clock::time_point scheduledTime_;
    bool hasScheduledUpdate_ = false;

    DownloadClient downloadClient_;
    DaemonController daemonController_;
    std::shared_ptr<RelayControlImpl> relayControlStub_;
    bool relayServiceRegistered_ = false;

    void setState(relay::RelayState newState, uint32_t versionId, const std::string& message, uint32_t progress = 0);
    bool handleCommand(uint32_t commandCode, uint32_t versionId, uint32_t parameter, std::string& outMessage);
    void handleVersionQuery(uint32_t& versionId, std::string& versionString);

    void checkAndDownloadUpdates();
    void processScheduledCommands();
    bool doDownload(uint32_t versionId);
    bool doUpdateScheduled(uint32_t versionId, uint32_t delaySec);
    bool doInstall();
    bool sendToDaemon();
    bool daemonInstall();
    bool triggerDaemonInstall(uint32_t versionId);
    void onDaemonProgress(uint32_t versionId, uint32_t progress,
                          const std::string& state, const std::string& message);
};

bool OtaRelay::loadConfigFromFile(const std::string& configPath) {
    try {
        std::ifstream file(configPath);
        if (!file.is_open()) {
            std::cerr << "[Relay] Failed to open config: " << configPath << std::endl;
            return false;
        }
        json j = json::parse(file);
        file.close();

        if (j.contains("downloadPath"))
            config_.downloadPath = j["downloadPath"].get<std::string>();
        if (j.contains("decompressionPath"))
            config_.decompressionPath = j["decompressionPath"].get<std::string>();
        if (j.contains("checkIntervalSec"))
            config_.checkIntervalSec = j["checkIntervalSec"].get<uint32_t>();
        if (j.contains("autoCleanup"))
            config_.autoCleanup = j["autoCleanup"].get<bool>();
        if (j.contains("serviceDomain"))
            config_.serviceDomain = j["serviceDomain"].get<std::string>();
        if (j.contains("serviceInstance"))
            config_.serviceInstance = j["serviceInstance"].get<std::string>();
        if (j.contains("relayDomain"))
            config_.relayDomain = j["relayDomain"].get<std::string>();
        if (j.contains("relayInstance"))
            config_.relayInstance = j["relayInstance"].get<std::string>();
        if (j.contains("daemonDomain"))
            config_.daemonDomain = j["daemonDomain"].get<std::string>();
        if (j.contains("daemonInstance"))
            config_.daemonInstance = j["daemonInstance"].get<std::string>();

        std::filesystem::create_directories(config_.downloadPath);
        std::filesystem::create_directories(config_.decompressionPath);

        std::cout << "[Relay] Config loaded:" << std::endl;
        std::cout << "  downloadPath: " << config_.downloadPath << std::endl;
        std::cout << "  decompressionPath: " << config_.decompressionPath << std::endl;
        std::cout << "  checkInterval: " << config_.checkIntervalSec << "s" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Relay] Error loading config: " << e.what() << std::endl;
        return false;
    }
}

bool OtaRelay::init() {
    std::cout << "[Relay] Initializing..." << std::endl;

    runtime_ = CommonAPI::Runtime::get();
    if (!runtime_) {
        std::cerr << "[Relay] Failed to get CommonAPI Runtime" << std::endl;
        return false;
    }

    std::cout << "[Relay] Will connect to service/daemon in background..." << std::endl;
    downloadClient_.connectAsync();
    daemonController_.connectAsync();

    daemonController_.subscribeToProgress(
        [this](uint32_t vid, uint32_t progress, const std::string& state, const std::string& msg) {
            onDaemonProgress(vid, progress, state, msg);
        }
    );

    relayControlStub_ = std::make_shared<RelayControlImpl>();
    relayControlStub_->setCommandHandler(
        [this](uint32_t code, uint32_t vid, uint32_t param, std::string& outMsg) {
            return handleCommand(code, vid, param, outMsg);
        }
    );
    relayControlStub_->setVersionQueryHandler(
        [this](uint32_t& vid, std::string& vs) {
            handleVersionQuery(vid, vs);
        }
    );

    bool registered = runtime_->registerService(config_.relayDomain,
                                                 config_.relayInstance,
                                                 relayControlStub_);
    if (!registered) {
        std::cerr << "[Relay] Failed to register RelayControl service" << std::endl;
        return false;
    }
    relayServiceRegistered_ = true;
    std::cout << "[Relay] RelayControl service registered" << std::endl;

    setState(relay::RelayState::IDLE, 0, "Relay started");
    return true;
}

void OtaRelay::setState(relay::RelayState newState, uint32_t versionId,
                         const std::string& message, uint32_t progress) {
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        currentState_ = newState;
    }

    if (relayControlStub_) {
        relayControlStub_->notifyStateChanged(
            relay::relayStateToString(newState), progress, versionId, message);
    }

    std::cout << "[Relay] State: " << relay::relayStateToString(newState)
              << " (" << progress << "%) | " << message << std::endl;
}

bool OtaRelay::handleCommand(uint32_t commandCode, uint32_t versionId,
                              uint32_t parameter, std::string& outMessage) {
    switch (static_cast<relay::CommandCode>(commandCode)) {
        case relay::UPDATE_NOW:
            if (doDownload(versionId)) {
                outMessage = "Download complete, ready to install";
                return true;
            }
            outMessage = "Failed to download update";
            return false;

        case relay::UPDATE_SCHEDULED:
            if (doUpdateScheduled(versionId, parameter)) {
                outMessage = "Update scheduled in " + std::to_string(parameter) + " seconds";
                return true;
            }
            outMessage = "Failed to schedule update";
            return false;

        case relay::GET_VERSION:
            outMessage = "Current version: " + currentVersionString_;
            return true;

        case relay::CANCEL:
            {
                relay::RelayState current;
                {
                    std::lock_guard<std::mutex> lock(stateMutex_);
                    current = currentState_;
                }
                if (current == relay::RelayState::INSTALLING) {
                    std::string daemonMsg;
                    if (daemonController_.cancelInstall(daemonMsg)) {
                        setState(relay::RelayState::IDLE, pendingVersionId_, "Install cancelled");
                        outMessage = "Install cancelled";
                        return true;
                    }
                    outMessage = "Cancel failed: " + daemonMsg;
                    return false;
                }
                hasScheduledUpdate_ = false;
                setState(relay::RelayState::IDLE, 0, "Operation cancelled");
                outMessage = "Cancelled";
                return true;
            }

        case relay::GET_STATUS:
            {
                std::lock_guard<std::mutex> lock(stateMutex_);
                outMessage = std::string("State: ") + relay::relayStateToString(currentState_)
                           + ", Version: " + currentVersionString_;
            }
            return true;

        case relay::INSTALL:
            if (doInstall()) {
                outMessage = "Installation triggered";
                return true;
            }
            outMessage = "Nothing to install (no file ready)";
            return false;

        case relay::SEND_TO_DAEMON:
            if (sendToDaemon()) {
                outMessage = "File sent to daemon";
                return true;
            }
            outMessage = "Failed to send file to daemon";
            return false;

        case relay::DAEMON_INSTALL:
            if (daemonInstall()) {
                outMessage = "Install triggered on daemon";
                return true;
            }
            outMessage = "Failed to trigger install on daemon";
            return false;

        default:
            outMessage = "Unknown command code: " + std::to_string(commandCode);
            return false;
    }
}

void OtaRelay::handleVersionQuery(uint32_t& versionId, std::string& versionString) {
    versionId = currentVersionId_;
    versionString = currentVersionString_;
}

bool OtaRelay::doDownload(uint32_t versionId) {
    relay::RelayState current;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        current = currentState_;
    }

    if (current == relay::RelayState::DOWNLOADING ||
        current == relay::RelayState::INSTALLING) {
        std::cerr << "[Relay] Busy, cannot start download" << std::endl;
        return false;
    }

    hasScheduledUpdate_ = false;

    if (versionId == 0) {
        uint32_t svid = 0;
        int64_t size = 0;
        std::string md5;
        bool comp = false;

        for (int retry = 0; retry < 10; ++retry) {
            if (downloadClient_.checkForUpdate(svid, size, md5, comp)) break;
            std::cerr << "[Relay] Service not available, retrying... (" << (retry + 1) << "/10)" << std::endl;
            std::this_thread::sleep_for(2s);
        }
        if (svid == 0) {
            std::cerr << "[Relay] No update available from service" << std::endl;
            return false;
        }
        versionId = svid;
    }

    if (pendingVersionId_ == versionId && current == relay::RelayState::READY) {
        std::cout << "[Relay] Already downloaded version 0x" << std::hex << versionId << std::dec << std::endl;
        return true;
    }

    bool downloaded = downloadClient_.downloadUpdate(
        versionId, config_.downloadPath,
        [this, versionId](uint32_t vid, uint32_t pct, const std::string& status) {
            (void)vid;
            (void)status;
            setState(relay::RelayState::DOWNLOADING, versionId, "Downloading... " + std::to_string(pct) + "%", pct);
        },
        [this, versionId](bool success, const std::string& filePath, const std::string& message) {
            (void)message;
            if (success) {
                pendingVersionId_ = versionId;
                pendingFilePath_ = filePath;
                downloadedVersionId_ = versionId;
            }
        }
    );

    if (!downloaded) {
        setState(relay::RelayState::ERROR, versionId, "Download failed");
        return false;
    }

    setState(relay::RelayState::READY, versionId, "Download complete, ready to install", 100);
    return true;
}

bool OtaRelay::doInstall() {
    relay::RelayState current;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        current = currentState_;
    }

    if (current != relay::RelayState::READY) {
        std::cerr << "[Relay] No downloaded file ready to install" << std::endl;
        return false;
    }

    if (pendingFilePath_.empty()) {
        std::cerr << "[Relay] No file path available" << std::endl;
        return false;
    }

    return triggerDaemonInstall(pendingVersionId_);
}

bool OtaRelay::sendToDaemon() {
    if (pendingFilePath_.empty()) {
        std::cerr << "[Relay] No file to send to daemon" << std::endl;
        return false;
    }
    if (!triggerDaemonInstall(pendingVersionId_)) {
        return false;
    }
    setState(relay::RelayState::IDLE, pendingVersionId_, "File sent to daemon, waiting for install command");
    return true;
}

bool OtaRelay::daemonInstall() {
    relay::RelayState current;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        current = currentState_;
    }
    std::string msg;
    if (!daemonController_.triggerInstall(msg)) {
        std::cerr << "[Relay] Failed to trigger install on daemon: " << msg << std::endl;
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        setState(relay::RelayState::INSTALLING, currentVersionId_, "Daemon installing...");
    }
    return true;
}

bool OtaRelay::doUpdateScheduled(uint32_t versionId, uint32_t delaySec) {
    relay::RelayState current;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        current = currentState_;
    }

    if (current == relay::RelayState::DOWNLOADING ||
        current == relay::RelayState::INSTALLING) {
        std::cerr << "[Relay] Busy, cannot schedule update" << std::endl;
        return false;
    }

    hasScheduledUpdate_ = true;
    scheduledTime_ = std::chrono::steady_clock::now() + std::chrono::seconds(delaySec);
    pendingVersionId_ = versionId;

    setState(relay::RelayState::SCHEDULED, versionId,
             "Update scheduled in " + std::to_string(delaySec) + " seconds");
    return true;
}

bool OtaRelay::triggerDaemonInstall(uint32_t versionId) {
    std::cout << "[Relay] triggerDaemonInstall: version=0x" << std::hex << versionId
              << std::dec << ", file=" << pendingFilePath_ << std::endl;
    setState(relay::RelayState::INSTALLING, versionId, "Sending file to daemon");

    if (!daemonController_.isAvailable()) {
        std::cout << "[Relay] Daemon not available, waiting..." << std::endl;
        bool becameAvailable = false;
        for (int retry = 0; retry < 15; ++retry) {
            std::this_thread::sleep_for(2s);
            if (daemonController_.isAvailable()) {
                becameAvailable = true;
                break;
            }
            std::cout << "[Relay] Waiting for daemon... (" << (retry + 1) << "/15)" << std::endl;
        }
        if (!becameAvailable) {
            std::cerr << "[Relay] Daemon proxy is not available!" << std::endl;
            setState(relay::RelayState::ERROR, versionId, "Daemon not reachable");
            return false;
        }
    }

    if (!std::filesystem::exists(pendingFilePath_)) {
        std::cerr << "[Relay] Firmware file not found: " << pendingFilePath_ << std::endl;
        setState(relay::RelayState::ERROR, versionId, "File not found");
        return false;
    }

    uint64_t fileSize = std::filesystem::file_size(pendingFilePath_);
    std::string md5Hash = computeFileMd5(pendingFilePath_);
    bool isCompressed = (pendingFilePath_.find(".bz2") != std::string::npos ||
                         pendingFilePath_.find(".gz") != std::string::npos);

    std::cout << "[Relay] Sending file: size=" << fileSize << ", md5=" << md5Hash
              << ", compressed=" << (isCompressed ? "yes" : "no") << std::endl;

    std::string daemonMsg;
    if (!daemonController_.sendFile(pendingFilePath_, versionId, fileSize, md5Hash, isCompressed, daemonMsg)) {
        std::cerr << "[Relay] Daemon sendFile failed: " << daemonMsg << std::endl;
        setState(relay::RelayState::ERROR, versionId, "Daemon install failed: " + daemonMsg);
        return false;
    }

    std::cout << "[Relay] Daemon sendFile completed: " << daemonMsg << std::endl;
    setState(relay::RelayState::INSTALLING, versionId, "Daemon installing...");
    return true;
}

void OtaRelay::onDaemonProgress(uint32_t versionId, uint32_t progress,
                                 const std::string& state, const std::string& message) {
    std::cout << "[Relay] Daemon progress: " << state << " " << progress << "% - " << message << std::endl;

    if (state == "complete") {
        currentVersionId_ = versionId;
        std::ostringstream ss;
        ss << "v" << ((versionId >> 16) & 0xFF) << "." << (versionId & 0xFF);
        currentVersionString_ = ss.str();

        if (config_.autoCleanup && !pendingFilePath_.empty()) {
            try {
                std::filesystem::remove(pendingFilePath_);
                std::cout << "[Relay] Cleaned up: " << pendingFilePath_ << std::endl;
            } catch (...) {}
        }

        downloadedVersionId_ = 0;
        pendingVersionId_ = 0;
        pendingFilePath_.clear();
        setState(relay::RelayState::COMPLETE, versionId, "Install complete: " + message, 100);
        setState(relay::RelayState::IDLE, versionId, "Ready");
    } else if (state == "failed" || state == "error") {
        setState(relay::RelayState::ERROR, versionId, "Install failed: " + message, progress);
    } else {
        setState(relay::RelayState::INSTALLING, versionId, message, progress);
    }
}

void OtaRelay::checkAndDownloadUpdates() {
    if (!downloadClient_.isAvailable()) return;

    relay::RelayState current;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        current = currentState_;
    }

    if (current != relay::RelayState::IDLE && current != relay::RelayState::READY) {
        return;
    }

    uint32_t versionId = 0;
    int64_t fileSize = 0;
    std::string md5Hash;
    bool isCompressed = false;

    if (!downloadClient_.checkForUpdate(versionId, fileSize, md5Hash, isCompressed)) {
        return;
    }

    if (versionId == 0 || versionId <= currentVersionId_ || versionId == downloadedVersionId_) {
        return;
    }

    std::cout << "[Relay] New update available: version 0x" << std::hex << versionId
              << std::dec << " (" << fileSize << " bytes)" << std::endl;

    setState(relay::RelayState::DOWNLOADING, versionId, "Downloading update...");

    bool downloaded = downloadClient_.downloadUpdate(
        versionId, config_.downloadPath,
        [this, versionId](uint32_t vid, uint32_t pct, const std::string& status) {
            (void)status;
            if (vid == versionId) {
                setState(relay::RelayState::DOWNLOADING, vid,
                         "Downloading... " + std::to_string(pct) + "%", pct);
            }
        },
        [this, versionId](bool success, const std::string& filePath, const std::string& message) {
            if (success) {
                pendingVersionId_ = versionId;
                pendingFilePath_ = filePath;
                downloadedVersionId_ = versionId;
                setState(relay::RelayState::READY, versionId,
                         "Download complete. Click 'Install' to decompress.", 100);
            } else {
                setState(relay::RelayState::ERROR, versionId,
                         "Download failed: " + message, 0);
            }
        }
    );

    if (!downloaded) {
        setState(relay::RelayState::IDLE, versionId, "Download skipped or failed");
    }
}

void OtaRelay::processScheduledCommands() {
    if (!hasScheduledUpdate_) return;

    auto now = std::chrono::steady_clock::now();
    if (now >= scheduledTime_) {
        std::cout << "[Relay] Scheduled download time reached" << std::endl;
        hasScheduledUpdate_ = false;
        doDownload(pendingVersionId_);
    }
}

void OtaRelay::run() {
    running_ = true;
    std::cout << "[Relay] Relay started. Waiting for commands..." << std::endl;

    unsigned int loopCount = 0;
    while (running_ && g_running) {
        try {
            processScheduledCommands();

            if (++loopCount % 10 == 0) {
                checkAndDownloadUpdates();
            }
        } catch (const std::exception& e) {
            std::cerr << "[Relay] Error in main loop: " << e.what() << std::endl;
        }

        std::this_thread::sleep_for(1s);
    }

    std::cout << "[Relay] Stopped" << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file.json>" << std::endl;
        std::cerr << "Example: " << argv[0] << " relay_config.json" << std::endl;
        return 1;
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    RelayConfig config;
    OtaRelay relay(config);

    if (!relay.loadConfigFromFile(argv[1])) {
        return 1;
    }

    if (!relay.init()) {
        std::cerr << "Failed to initialize relay" << std::endl;
        return 1;
    }

    std::cout << "\n=== OTA Relay Running (manual mode) ===" << std::endl;
    std::cout << "Use Download button to fetch, Install button to deploy" << std::endl;
    std::cout << "==========================================\n" << std::endl;

    relay.run();
    return 0;
}

