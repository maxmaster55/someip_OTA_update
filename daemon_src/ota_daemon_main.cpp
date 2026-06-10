#include <iostream>
#include "CommonAPI/CommonAPI.hpp"
#include <v1/manager/updater/UpdaterProxy.hpp>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <openssl/md5.h>
#include <filesystem>
#include <map>
#include "transfer_config.hpp"
#include "config_manager.hpp"
#include "file_decompressor.hpp"

using namespace std::chrono_literals;

class UpdateDaemon {
public:
    UpdateDaemon(const ClientConfig::Settings& config)
        : config_(config), running_(true), lastVersionProcessed_(0) {}

    bool connect();
    void run();
    void stop() { running_ = false; }

private:
    ClientConfig::Settings config_;
    std::shared_ptr<v1::manager::updater::UpdaterProxy<>> proxy_;
    bool running_;
    uint32_t lastVersionProcessed_;

    std::string calculateMD5(const std::vector<uint8_t>& data);
    bool downloadUpdate(uint32_t versionId, const std::string& filename);
    bool decompressUpdate(const std::string& filename);
    void checkForUpdates();
    void processUpdate(uint32_t versionId, const std::string& filename);
};

static std::string md5DigestString(const unsigned char* digest) {
    std::stringstream ss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }
    return ss.str();
}

std::string UpdateDaemon::calculateMD5(const std::vector<uint8_t>& data) {
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5_CTX md5Context;
    MD5_Init(&md5Context);
    MD5_Update(&md5Context, data.data(), data.size());
    MD5_Final(digest, &md5Context);
    return md5DigestString(digest);
}

bool UpdateDaemon::connect() {
    auto runtime = CommonAPI::Runtime::get();
    if (!runtime) {
        std::cerr << "Failed to get CommonAPI Runtime instance" << std::endl;
        return false;
    }

    for (int i = 0;; ++i) {
        proxy_ = runtime->buildProxy<v1::manager::updater::UpdaterProxy>(
            config_.serviceDomain, config_.serviceInstance);

        if (proxy_ && proxy_->isAvailable()) {
            std::cout << "[" << std::chrono::system_clock::now().time_since_epoch().count() << "] "
                      << "Connected to Update_Notifier service" << std::endl;
            return true;
        }

            std::cout << "[" << std::chrono::system_clock::now().time_since_epoch().count() << "] "
                      << "Waiting for service... (attempt " << (i + 1) << ")" << std::endl;
            std::this_thread::sleep_for(2s);
    }
}

bool UpdateDaemon::downloadUpdate(uint32_t versionId, const std::string& filename) {
    std::cout << "[DOWNLOAD] Starting download for version 0x" << std::hex << versionId 
              << std::dec << " (" << filename << ")" << std::endl;

    // Request update info
    uint32_t fetchedVersionId = 0;
    int64_t fileSize = 0;
    std::string md5Hash = "";
    bool isCompressed = false;

    CommonAPI::CallStatus callStatus;
    proxy_->getUpdateInfo(callStatus, fetchedVersionId, fileSize, md5Hash, isCompressed);

    if (callStatus != CommonAPI::CallStatus::SUCCESS) {
        std::cerr << "[ERROR] Failed to get update info" << std::endl;
        proxy_->getDownloadStatus(versionId, false, true, "Failed to get update info", callStatus);
        return false;
    }

    std::cout << "[DOWNLOAD] Update info: Size=" << fileSize << ", MD5=" << md5Hash 
              << ", Compressed=" << (isCompressed ? "yes" : "no") 
              << ", Window=" << WINDOW_SIZE << ", ChunkSize=" << CHUNK_SIZE << std::endl;

    uint32_t totalChunks = (static_cast<uint32_t>(fileSize) + CHUNK_SIZE - 1) / CHUNK_SIZE;
    if (totalChunks == 0) {
        std::cout << "[DOWNLOAD] Empty file, nothing to download" << std::endl;
        return true;
    }

    // Open output file and pre-allocate
    std::string filePath = config_.downloadPath + "/" + filename;
    std::ofstream outFile(filePath, std::ios::binary);
    if (!outFile) {
        std::cerr << "[ERROR] Cannot create output file: " << filePath << std::endl;
        return false;
    }
    outFile.seekp(static_cast<std::streamoff>(fileSize - 1));
    outFile.write("", 1);
    outFile.seekp(0);

    // Streaming MD5 context
    MD5_CTX md5Context;
    MD5_Init(&md5Context);

    // Sliding window state
    std::mutex mtx;
    std::condition_variable cv;
    uint32_t nextToSend = 0;
    uint32_t outstanding = 0;
    uint32_t chunksReceived = 0;
    bool failed = false;

    auto callback = [&](const CommonAPI::CallStatus& status, const uint32_t& recvIdx,
                        const std::string& data, const bool& lastChunk) {
        std::lock_guard<std::mutex> lock(mtx);
        if (status != CommonAPI::CallStatus::SUCCESS) {
            std::cerr << "[ERROR] Failed to download chunk " << recvIdx << std::endl;
            failed = true;
        } else {
            size_t offset = static_cast<size_t>(recvIdx) * CHUNK_SIZE;
            outFile.seekp(offset);
            outFile.write(data.data(), static_cast<std::streamsize>(data.size()));
            MD5_Update(&md5Context, data.data(), data.size());
            chunksReceived++;
        }
        outstanding--;
        cv.notify_one();
    };

    auto t0 = std::chrono::steady_clock::now();

    // Seed initial window
    {
        std::unique_lock<std::mutex> lock(mtx);
        while (nextToSend < totalChunks && outstanding < WINDOW_SIZE) {
            uint32_t idx = nextToSend++;
            outstanding++;
            lock.unlock();
            proxy_->requestDataAsync(versionId, idx, callback);
            lock.lock();
        }
    }

    std::cout << "[DOWNLOAD] Downloading " << totalChunks << " chunks with window " << WINDOW_SIZE << "..." << std::endl;

    // Wait for completion and refill window
    {
        std::unique_lock<std::mutex> lock(mtx);
        while (chunksReceived < totalChunks && !failed) {
            cv.wait(lock);
            while (nextToSend < totalChunks && outstanding < WINDOW_SIZE && !failed) {
                uint32_t idx = nextToSend++;
                outstanding++;
                lock.unlock();
                proxy_->requestDataAsync(versionId, idx, callback);
                lock.lock();
            }
        }
    }

    outFile.close();

    if (failed) {
        std::cerr << "[ERROR] Download failed after " << chunksReceived << "/" << totalChunks << " chunks" << std::endl;
        std::filesystem::remove(filePath);
        proxy_->getDownloadStatus(versionId, false, true, "Download failed", callStatus);
        return false;
    }

    auto t1 = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();

    std::cout << "[DOWNLOAD] All " << totalChunks << " chunks received (" << fileSize << " bytes)" << std::endl;
    std::cout << "[DOWNLOAD] Time: " << elapsed << " s, Speed: "
              << (fileSize / 1'048'576.0 / elapsed) << " MB/s" << std::endl;

    // Verify checksum
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5_Final(digest, &md5Context);
    std::string downloadedMD5 = md5DigestString(digest);
    bool checksumMatch = (downloadedMD5 == md5Hash);
    bool sizeMatch = (static_cast<int64_t>(std::filesystem::file_size(filePath)) == fileSize);

    std::cout << "[VERIFY] Downloaded MD5: " << downloadedMD5 << std::endl;
    std::cout << "[VERIFY] Expected MD5:   " << md5Hash << std::endl;
    std::cout << "[VERIFY] Size match: " << (sizeMatch ? "yes" : "no") << std::endl;

    if (!checksumMatch || !sizeMatch) {
        std::cerr << "[ERROR] Checksum or size verification failed!" << std::endl;
        std::filesystem::remove(filePath);
        proxy_->getDownloadStatus(versionId, false, true, "Verification failed", callStatus);
        return false;
    }

    std::cout << "[SUCCESS] File saved to: " << filePath << std::endl;

    proxy_->getDownloadStatus(versionId, true, false, "Download successful", callStatus);
    return true;
}

bool UpdateDaemon::decompressUpdate(const std::string& filename) {
    if (!config_.autoDecompress) {
        std::cout << "[DECOMPRESS] Auto-decompress disabled, skipping" << std::endl;
        return true;
    }

    std::string format;
    if (!Decompressor::detectFormat(filename, format)) {
        std::cout << "[DECOMPRESS] File is not compressed, no decompression needed" << std::endl;
        return true;
    }

    std::cout << "[DECOMPRESS] Decompressing " << format << " file..." << std::endl;

    std::string inputPath = config_.downloadPath + "/" + filename;
    std::string errorMsg;

    if (!Decompressor::decompress(inputPath, config_.decompressionPath, filename, errorMsg)) {
        std::cerr << "[ERROR] Decompression failed: " << errorMsg << std::endl;
        return false;
    }

    std::cout << "[SUCCESS] Decompressed to: " << config_.decompressionPath << std::endl;

    // Cleanup compressed file if enabled
    if (config_.autoCleanup) {
        std::cout << "[CLEANUP] Removing compressed file: " << inputPath << std::endl;
        try {
            std::filesystem::remove(inputPath);
        } catch (const std::exception& e) {
            std::cerr << "[WARNING] Failed to cleanup: " << e.what() << std::endl;
        }
    }

    return true;
}

void UpdateDaemon::processUpdate(uint32_t versionId, const std::string& filename) {
    if (versionId <= lastVersionProcessed_) {
        std::cout << "[DAEMON] Version already processed, skipping" << std::endl;
        return;
    }

    std::cout << "[DAEMON] Processing update: version 0x" << std::hex << versionId 
              << std::dec << ", file=" << filename << std::endl;

    if (!downloadUpdate(versionId, filename)) {
        std::cerr << "[ERROR] Download failed" << std::endl;
        return;
    }

    if (!decompressUpdate(filename)) {
        std::cerr << "[ERROR] Decompression failed" << std::endl;
        // Still record that we processed this version
        lastVersionProcessed_ = versionId;
        return;
    }

    std::cout << "[SUCCESS] Update processed successfully!" << std::endl;
    lastVersionProcessed_ = versionId;
}

void UpdateDaemon::checkForUpdates() {
    std::cout << "[DAEMON] Checking for updates..." << std::endl;

    // Get update info
    uint32_t versionId = 0;
    int64_t fileSize = 0;
    std::string md5Hash = "";
    bool isCompressed = false;

    CommonAPI::CallStatus callStatus;
    proxy_->getUpdateInfo(callStatus, versionId, fileSize, md5Hash, isCompressed);

    if (callStatus != CommonAPI::CallStatus::SUCCESS) {
        std::cout << "[DAEMON] Service not available, will retry..." << std::endl;
        return;
    }

    // Check if this is a new version
    if (versionId <= lastVersionProcessed_) {
        std::cout << "[DAEMON] Already have version 0x" << std::hex << versionId << std::dec << ", skipping" << std::endl;
        return;
    }

    // Found new version - extract filename and download
    std::cout << "[DAEMON] Found new update - version 0x" << std::hex << versionId << std::dec << std::endl;
    
    // Construct expected filename from version
    // For now, try to download with a generic name
    std::string filename = "file_ota_update.bin";
    if (isCompressed) {
        filename += ".bz2";  // Assume bzip2
    }

    processUpdate(versionId, filename);
}

void UpdateDaemon::run() {
    std::cout << "[DAEMON] Starting Update Daemon" << std::endl;
    std::cout << "[DAEMON] Listening for updates every " << config_.checkIntervalSec << " seconds" << std::endl;

    while (running_) {
        try {
            checkForUpdates();
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Exception in daemon loop: " << e.what() << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(config_.checkIntervalSec));
    }

    std::cout << "[DAEMON] Stopping" << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file.json>" << std::endl;
        std::cerr << "Example: " << argv[0] << " update_daemon.json" << std::endl;
        return 1;
    }

    std::string configFile = argv[1];

    // Load configuration
    ClientConfig config;
    if (!config.loadFromFile(configFile)) {
        std::cerr << "Failed to load configuration from: " << configFile << std::endl;
        return 1;
    }

    const auto& settings = config.getSettings();

    // Create and start daemon
    UpdateDaemon daemon(settings);

    if (!daemon.connect()) {
        std::cerr << "Failed to connect to Update service" << std::endl;
        return 1;
    }

    std::cout << "\n=== Update Daemon Running ===" << std::endl;
    std::cout << "Download Path: " << settings.downloadPath << std::endl;
    std::cout << "Decompress Path: " << settings.decompressionPath << std::endl;
    std::cout << "Check Interval: " << settings.checkIntervalSec << " seconds" << std::endl;
    std::cout << "Auto Decompress: " << (settings.autoDecompress ? "enabled" : "disabled") << std::endl;
    std::cout << "Auto Cleanup: " << (settings.autoCleanup ? "enabled" : "disabled") << std::endl;
    std::cout << "==============================\n" << std::endl;

    daemon.run();

    return 0;
}


