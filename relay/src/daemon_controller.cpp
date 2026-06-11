#include "relay/daemon_controller.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <vector>
#include <cstring>
#include <shared/transfer_config.hpp>

using namespace std::chrono_literals;

DaemonController::DaemonController(const std::string& domain, const std::string& instance)
    : domain_(domain), instance_(instance) {}

bool DaemonController::connect() {
    runtime_ = CommonAPI::Runtime::get();
    if (!runtime_) {
        std::cerr << "[DaemonController] Failed to get CommonAPI Runtime" << std::endl;
        return false;
    }

    for (int i = 0; i < MAX_CONNECT_RETRIES; ++i) {
        proxy_ = runtime_->buildProxy<v1::manager::updater::DaemonControlProxy>(domain_, instance_);
        if (proxy_ && proxy_->isAvailable()) {
            std::cout << "[DaemonController] Connected to Daemon service" << std::endl;
            return true;
        }
        std::cout << "[DaemonController] Waiting for daemon... (attempt " << (i + 1) << "/" << MAX_CONNECT_RETRIES << ")" << std::endl;
        std::this_thread::sleep_for(2s);
    }

    std::cerr << "[DaemonController] Failed to connect to Daemon service after " << MAX_CONNECT_RETRIES << " attempts" << std::endl;
    return false;
}

void DaemonController::connectAsync() {
    runtime_ = CommonAPI::Runtime::get();
    if (!runtime_) {
        std::cerr << "[DaemonController] Failed to get CommonAPI Runtime" << std::endl;
        return;
    }
    proxy_ = runtime_->buildProxy<v1::manager::updater::DaemonControlProxy>(domain_, instance_);
    if (proxy_) {
        proxy_->getProxyStatusEvent().subscribe(
            [this](const CommonAPI::AvailabilityStatus& status) {
                if (status == CommonAPI::AvailabilityStatus::AVAILABLE) {
                    std::cout << "[DaemonController] Daemon now available" << std::endl;
                }
            }
        );
    }
}

bool DaemonController::isAvailable() const {
    return proxy_ && proxy_->isAvailable();
}

bool DaemonController::sendFile(const std::string& firmwarePath, uint32_t versionId,
                                 uint64_t fileSize, const std::string& md5Hash, bool isCompressed,
                                 std::string& outMessage) {
    if (!proxy_ || !proxy_->isAvailable()) {
        outMessage = "Daemon proxy not available";
        return false;
    }

    std::cout << "[DaemonController] beginInstall: version=0x" << std::hex << versionId
              << std::dec << ", size=" << fileSize << ", md5=" << md5Hash << std::endl;

    CommonAPI::CallStatus callStatus;
    bool accepted = false;
    std::string message;

    proxy_->beginInstall(versionId, fileSize, md5Hash, isCompressed, callStatus, accepted, message);
    if (callStatus != CommonAPI::CallStatus::SUCCESS || !accepted) {
        outMessage = "Daemon rejected: " + message;
        std::cerr << "[DaemonController] beginInstall failed: " << outMessage << std::endl;
        return false;
    }

    std::ifstream inFile(firmwarePath, std::ios::binary);
    if (!inFile) {
        outMessage = "Cannot open firmware file: " + firmwarePath;
        return false;
    }

    uint32_t chunkIndex = 0;
    std::vector<char> buf(CHUNK_SIZE);

    while (inFile.read(buf.data(), buf.size()) || inFile.gcount()) {
        std::streamsize bytesRead = inFile.gcount();
        std::string chunkData(buf.data(), static_cast<size_t>(bytesRead));

        proxy_->sendChunk(versionId, chunkIndex, chunkData, callStatus, accepted);
        if (callStatus != CommonAPI::CallStatus::SUCCESS || !accepted) {
            outMessage = "Chunk " + std::to_string(chunkIndex) + " failed";
            std::cerr << "[DaemonController] sendChunk " << chunkIndex << " failed" << std::endl;
            return false;
        }

        chunkIndex++;
        if (chunkIndex % 1000 == 0) {
            std::cout << "[DaemonController] Sent " << chunkIndex << " chunks..." << std::endl;
        }
    }

    inFile.close();
    std::cout << "[DaemonController] Sent " << chunkIndex << " chunks total" << std::endl;

    proxy_->finishInstall(versionId, callStatus, accepted, message);
    outMessage = message;

    if (callStatus != CommonAPI::CallStatus::SUCCESS || !accepted) {
        std::cerr << "[DaemonController] finishInstall failed: " << message << std::endl;
        return false;
    }

    std::cout << "[DaemonController] Install completed: " << message << std::endl;
    return true;
}

bool DaemonController::triggerInstall(std::string& outMessage) {
    if (!proxy_ || !proxy_->isAvailable()) {
        outMessage = "Daemon proxy not available";
        return false;
    }

    CommonAPI::CallStatus callStatus;
    bool accepted = false;
    std::string message;

    proxy_->beginInstall(0, 0, "", false, callStatus, accepted, message);
    outMessage = message;

    if (callStatus != CommonAPI::CallStatus::SUCCESS || !accepted) {
        std::cerr << "[DaemonController] triggerInstall failed: " << message << std::endl;
        return false;
    }

    std::cout << "[DaemonController] Install triggered on daemon: " << message << std::endl;
    return true;
}

bool DaemonController::cancelInstall(std::string& outMessage) {
    if (!proxy_) {
        outMessage = "Daemon proxy not available";
        return false;
    }

    CommonAPI::CallStatus callStatus;
    bool accepted = false;

    proxy_->cancelInstall(callStatus, accepted);
    outMessage = accepted ? "Cancel accepted" : "Cancel rejected";

    return accepted;
}

void DaemonController::subscribeToProgress(ProgressCallback callback) {
    progressCallback_ = callback;
    if (!proxy_) return;

    proxy_->getInstallProgressEvent().subscribe(
        [this](const uint32_t& versionId, const uint32_t& progress,
               const std::string& state, const std::string& message) {
            if (progressCallback_) {
                progressCallback_(versionId, progress, state, message);
            }
        }
    );
}

