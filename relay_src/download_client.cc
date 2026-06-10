#include "download_client.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstring>
#include <openssl/md5.h>
#include <sstream>
#include <iomanip>
#include <chrono>

using namespace std::chrono_literals;

static std::string md5DigestString(const unsigned char* digest) {
    std::stringstream ss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }
    return ss.str();
}

DownloadClient::DownloadClient(const std::string& domain, const std::string& instance)
    : domain_(domain), instance_(instance) {}

bool DownloadClient::connect() {
    runtime_ = CommonAPI::Runtime::get();
    if (!runtime_) {
        std::cerr << "[DownloadClient] Failed to get CommonAPI Runtime" << std::endl;
        return false;
    }

    for (int i = 0;; ++i) {
        proxy_ = runtime_->buildProxy<v1::manager::updater::UpdaterProxy>(domain_, instance_);
        if (proxy_ && proxy_->isAvailable()) {
            std::cout << "[DownloadClient] Connected to Update service" << std::endl;
            return true;
        }
        std::cout << "[DownloadClient] Waiting for service... (attempt " << (i + 1) << ")" << std::endl;
        std::this_thread::sleep_for(2s);
    }
}

bool DownloadClient::isAvailable() const {
    return proxy_ && proxy_->isAvailable();
}

bool DownloadClient::checkForUpdate(uint32_t& versionId, int64_t& fileSize,
                                     std::string& md5Hash, bool& isCompressed) {
    if (!proxy_) return false;

    CommonAPI::CallStatus callStatus;
    proxy_->getUpdateInfo(callStatus, versionId, fileSize, md5Hash, isCompressed);

    if (callStatus != CommonAPI::CallStatus::SUCCESS) {
        return false;
    }
    return true;
}

bool DownloadClient::downloadUpdate(uint32_t versionId, const std::string& downloadPath,
                                    ProgressCallback onProgress, CompletionCallback onComplete) {
    std::cout << "[DownloadClient] Starting download for version 0x" << std::hex << versionId << std::dec << std::endl;

    uint32_t fetchedVersionId = 0;
    int64_t fileSize = 0;
    std::string md5Hash = "";
    bool isCompressed = false;

    CommonAPI::CallStatus callStatus;
    proxy_->getUpdateInfo(callStatus, fetchedVersionId, fileSize, md5Hash, isCompressed);

    if (callStatus != CommonAPI::CallStatus::SUCCESS) {
        std::cerr << "[DownloadClient] Failed to get update info" << std::endl;
        proxy_->getDownloadStatus(versionId, false, true, "Failed to get update info", callStatus);
        if (onComplete) onComplete(false, "", "Failed to get update info");
        return false;
    }

    std::cout << "[DownloadClient] Update info: Size=" << fileSize << ", MD5=" << md5Hash
              << ", Compressed=" << (isCompressed ? "yes" : "no") << std::endl;

    uint32_t totalChunks = (static_cast<uint32_t>(fileSize) + CHUNK_SIZE - 1) / CHUNK_SIZE;
    if (totalChunks == 0) {
        std::cout << "[DownloadClient] Empty file, nothing to download" << std::endl;
        lastFilePath_ = "";
        if (onComplete) onComplete(true, "", "Empty file");
        return true;
    }

    std::string filename = "file_ota_update.bin";
    if (isCompressed) {
        filename += ".bz2";
    }
    std::string filePath = downloadPath + "/" + filename;

    std::filesystem::create_directories(downloadPath);

    std::ofstream outFile(filePath, std::ios::binary);
    if (!outFile) {
        std::cerr << "[DownloadClient] Cannot create output file: " << filePath << std::endl;
        if (onComplete) onComplete(false, "", "Cannot create output file");
        return false;
    }
    outFile.seekp(static_cast<std::streamoff>(fileSize - 1));
    outFile.write("", 1);
    outFile.seekp(0);

    MD5_CTX md5Context;
    MD5_Init(&md5Context);

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
            std::cerr << "[DownloadClient] Failed to download chunk " << recvIdx << std::endl;
            failed = true;
        } else {
            size_t offset = static_cast<size_t>(recvIdx) * CHUNK_SIZE;
            outFile.seekp(offset);
            outFile.write(data.data(), static_cast<std::streamsize>(data.size()));
            MD5_Update(&md5Context, data.data(), data.size());
            chunksReceived++;

            if (onProgress) {
                uint32_t pct = static_cast<uint32_t>((static_cast<double>(chunksReceived) / totalChunks) * 100);
                onProgress(versionId, pct, "downloading");
            }
        }
        outstanding--;
        cv.notify_one();
    };

    auto t0 = std::chrono::steady_clock::now();

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

    std::cout << "[DownloadClient] Downloading " << totalChunks << " chunks..." << std::endl;

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
        std::cerr << "[DownloadClient] Download failed after " << chunksReceived << "/" << totalChunks << " chunks" << std::endl;
        std::filesystem::remove(filePath);
        proxy_->getDownloadStatus(versionId, false, true, "Download failed", callStatus);
        if (onComplete) onComplete(false, "", "Download failed");
        return false;
    }

    auto t1 = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();

    std::cout << "[DownloadClient] All " << totalChunks << " chunks received (" << fileSize << " bytes)" << std::endl;
    std::cout << "[DownloadClient] Time: " << elapsed << " s, Speed: "
              << (fileSize / 1'048'576.0 / elapsed) << " MB/s" << std::endl;

    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5_Final(digest, &md5Context);
    std::string downloadedMD5 = md5DigestString(digest);
    bool checksumMatch = (downloadedMD5 == md5Hash);
    bool sizeMatch = (static_cast<int64_t>(std::filesystem::file_size(filePath)) == fileSize);

    std::cout << "[DownloadClient] Downloaded MD5: " << downloadedMD5 << std::endl;
    std::cout << "[DownloadClient] Expected MD5:   " << md5Hash << std::endl;

    if (!checksumMatch || !sizeMatch) {
        std::cerr << "[DownloadClient] Checksum or size verification failed!" << std::endl;
        std::filesystem::remove(filePath);
        proxy_->getDownloadStatus(versionId, false, true, "Verification failed", callStatus);
        if (onComplete) onComplete(false, "", "Verification failed");
        return false;
    }

    std::cout << "[DownloadClient] File saved to: " << filePath << std::endl;
    lastFilePath_ = filePath;

    proxy_->getDownloadStatus(versionId, true, false, "Download successful", callStatus);
    if (onComplete) onComplete(true, filePath, "Download successful");
    return true;
}
