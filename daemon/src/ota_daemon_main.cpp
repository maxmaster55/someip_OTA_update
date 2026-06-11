#include <iostream>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstring>
#include <csignal>
#include "CommonAPI/CommonAPI.hpp"
#include <v1/manager/updater/DaemonControlStubDefault.hpp>
#include <shared/file_decompressor.hpp>
#include <shared/config_manager.hpp>
#include <openssl/md5.h>

using namespace std::chrono_literals;

static std::atomic<bool> g_running{true};

static void signalHandler(int) {
    g_running = false;
}

class DaemonControlImpl : public v1::manager::updater::DaemonControlStubDefault {
public:
    explicit DaemonControlImpl(const std::string& downloadDir, const std::string& outputDir)
        : cancelled_(false), downloadDir_(downloadDir), outputDir_(outputDir) {
        std::cout << "[Daemon] Created with downloadDir=" << downloadDir
                  << ", outputDir=" << outputDir << std::endl;
    }

    void beginInstall(const std::shared_ptr<CommonAPI::ClientId> _client,
                       uint32_t _versionId, uint64_t _fileSize,
                       std::string _md5Hash, bool _isCompressed,
                       beginInstallReply_t _reply) override {
        (void)_client;

        if (_fileSize == 0) {
            if (!storedFilePath_.empty()) {
                std::cout << "[Daemon] Triggering install for stored file: "
                          << storedFilePath_ << " (version 0x" << std::hex
                          << storedVersionId_ << std::dec << ")" << std::endl;
                std::string filePath = storedFilePath_;
                uint32_t verId = storedVersionId_;
                storedFilePath_.clear();
                storedVersionId_ = 0;
                fireInstallProgressEvent(verId, 0, "installing", "Starting install from stored file");
                std::thread([this, verId, filePath]() {
                    doInstall(verId, filePath);
                }).detach();
                _reply(true, "Install triggered");
            } else {
                _reply(false, "No file stored for installation");
            }
            return;
        }

        if (pendingVersionId_ != 0 && !cancelled_) {
            std::cerr << "[Daemon] Busy with version 0x" << std::hex << pendingVersionId_
                      << std::dec << ", rejecting new install" << std::endl;
            _reply(false, "Busy with another install");
            return;
        }

        std::filesystem::create_directories(downloadDir_);
        std::string path = downloadDir_ + "/incoming_0x" + std::to_string(_versionId) + ".bin";
        if (_isCompressed) path += ".bz2";

        std::cout << "[Daemon] beginInstall: version=0x" << std::hex << _versionId
                  << std::dec << ", size=" << _fileSize << ", md5=" << _md5Hash
                  << ", compressed=" << (_isCompressed ? "yes" : "no")
                  << ", path=" << path << std::endl;

        outFile_.open(path, std::ios::binary);
        if (!outFile_) {
            std::cerr << "[Daemon] Cannot create output file: " << path << std::endl;
            _reply(false, "Cannot create output file");
            return;
        }

        pendingVersionId_ = _versionId;
        pendingFilePath_ = path;
        pendingFileSize_ = _fileSize;
        pendingMd5_ = _md5Hash;
        pendingCompressed_ = _isCompressed;
        chunksReceived_ = 0;
        cancelled_ = false;

        MD5_Init(&md5Ctx_);
        std::cout << "[Daemon] Ready to receive " << _fileSize << " bytes into " << path << std::endl;
        _reply(true, "Ready to receive");
    }

    void sendChunk(const std::shared_ptr<CommonAPI::ClientId> _client,
                    uint32_t _versionId, uint32_t _chunkIndex,
                    std::string _data, sendChunkReply_t _reply) override {
        (void)_client;
        if (cancelled_ || _versionId != pendingVersionId_) {
            std::cerr << "[Daemon] sendChunk rejected: cancelled=" << cancelled_
                      << ", version mismatch (got 0x" << std::hex << _versionId
                      << ", expected 0x" << pendingVersionId_ << std::dec << ")" << std::endl;
            _reply(false);
            return;
        }

        outFile_.write(_data.data(), _data.size());
        MD5_Update(&md5Ctx_, _data.data(), _data.size());
        chunksReceived_++;

        if (chunksReceived_ % 1000 == 0) {
            std::cout << "[Daemon] Received " << chunksReceived_ << " chunks for version 0x"
                      << std::hex << _versionId << std::dec << std::endl;
        }

        _reply(true);
    }

    void finishInstall(const std::shared_ptr<CommonAPI::ClientId> _client,
                        uint32_t _versionId,
                        finishInstallReply_t _reply) override {
        (void)_client;
        std::cout << "[Daemon] finishInstall: version=0x" << std::hex << _versionId << std::dec << std::endl;

        if (_versionId != pendingVersionId_) {
            std::cerr << "[Daemon] finishInstall rejected: expected 0x"
                      << std::hex << pendingVersionId_ << ", got 0x" << _versionId
                      << std::dec << std::endl;
            _reply(false, "No pending install for this version");
            return;
        }

        uint32_t totalChunks = chunksReceived_;
        outFile_.close();
        std::cout << "[Daemon] File closed. Total chunks: " << totalChunks << std::endl;

        if (cancelled_) {
            std::cout << "[Daemon] Install was cancelled, cleaning up" << std::endl;
            cleanup();
            _reply(false, "Install cancelled");
            return;
        }

        uint64_t actualSize = std::filesystem::file_size(pendingFilePath_);
        std::cout << "[Daemon] Verifying size: expected=" << pendingFileSize_
                  << ", actual=" << actualSize << std::endl;
        if (actualSize != pendingFileSize_) {
            std::cerr << "[Daemon] Size mismatch: expected " << pendingFileSize_
                      << ", got " << actualSize << std::endl;
            cleanup();
            _reply(false, "File size mismatch");
            return;
        }
        std::cout << "[Daemon] Size verified OK (" << actualSize << " bytes)" << std::endl;

        if (!pendingMd5_.empty()) {
            unsigned char digest[MD5_DIGEST_LENGTH];
            MD5_Final(digest, &md5Ctx_);
            char hex[33];
            for (int i = 0; i < 16; i++)
                snprintf(hex + i * 2, 3, "%02x", digest[i]);
            std::string actualMd5(hex);
            std::cout << "[Daemon] Verifying MD5: expected=" << pendingMd5_
                      << ", actual=" << actualMd5 << std::endl;
            if (actualMd5 != pendingMd5_) {
                std::cerr << "[Daemon] MD5 mismatch: expected " << pendingMd5_
                          << ", got " << actualMd5 << std::endl;
                cleanup();
                _reply(false, "MD5 mismatch");
                return;
            }
            std::cout << "[Daemon] MD5 verified OK: " << actualMd5 << std::endl;
        } else {
            std::cout << "[Daemon] No MD5 hash provided, skipping verification" << std::endl;
        }

        std::cout << "[Daemon] File verification passed, storing for later install" << std::endl;
        storedFilePath_ = pendingFilePath_;
        storedVersionId_ = _versionId;
        pendingVersionId_ = 0;
        pendingFilePath_.clear();
        chunksReceived_ = 0;
        _reply(true, "File received successfully");
    }

    void cancelInstall(const std::shared_ptr<CommonAPI::ClientId> _client,
                       cancelInstallReply_t _reply) override {
        (void)_client;
        std::string filePath = pendingFilePath_;
        uint32_t versionId = pendingVersionId_;
        std::cout << "[Daemon] cancelInstall: version=0x" << std::hex << versionId
                  << std::dec << ", file=" << filePath << std::endl;
        cancelled_ = true;
        cleanup();
        std::cout << "[Daemon] Install cancelled and cleaned up" << std::endl;
        _reply(true);
    }

private:
    std::atomic<bool> cancelled_;
    std::string downloadDir_;
    std::string outputDir_;

    uint32_t pendingVersionId_ = 0;
    std::string pendingFilePath_;
    uint64_t pendingFileSize_ = 0;
    std::string pendingMd5_;
    bool pendingCompressed_ = false;
    uint32_t chunksReceived_ = 0;
    std::ofstream outFile_;
    MD5_CTX md5Ctx_;

    std::string storedFilePath_;
    uint32_t storedVersionId_ = 0;

    void cleanup() {
        std::string filePath = pendingFilePath_;
        if (outFile_.is_open()) {
            outFile_.close();
            std::cout << "[Daemon] Closed output file" << std::endl;
        }
        if (!filePath.empty()) {
            std::error_code ec;
            std::filesystem::remove(filePath, ec);
            if (!ec) {
                std::cout << "[Daemon] Removed partial file: " << filePath << std::endl;
            } else {
                std::cerr << "[Daemon] Failed to remove " << filePath << ": " << ec.message() << std::endl;
            }
        }
        pendingVersionId_ = 0;
        pendingFilePath_.clear();
        chunksReceived_ = 0;
    }

    void doInstall(uint32_t versionId, const std::string& firmwarePath) {
        std::cout << "[Daemon] doInstall started: version=0x" << std::hex << versionId
                  << std::dec << ", file=" << firmwarePath << std::endl;

        fireInstallProgressEvent(versionId, 0, "decompressing", "Starting decompression");

        if (cancelled_) {
            std::cout << "[Daemon] Install cancelled before start" << std::endl;
            fireInstallProgressEvent(versionId, 0, "cancelled", "Install cancelled before start");
            return;
        }

        std::string format;
        if (!Decompressor::detectFormat(firmwarePath, format)) {
            std::cout << "[Daemon] File is not compressed, install complete" << std::endl;
            fireInstallProgressEvent(versionId, 100, "complete",
                                     "File is not compressed, install complete");
            return;
        }
        std::cout << "[Daemon] Detected format: " << format << std::endl;

        std::filesystem::create_directories(outputDir_);

        std::string filename = std::filesystem::path(firmwarePath).filename().string();
        std::cout << "[Daemon] Output filename: " << filename << std::endl;
        std::string errorMsg;

        fireInstallProgressEvent(versionId, 30, "decompressing",
                                 "Decompressing " + format + " file...");

        if (cancelled_) {
            std::cout << "[Daemon] Install cancelled during decompression" << std::endl;
            fireInstallProgressEvent(versionId, 0, "cancelled", "Install cancelled during decompression");
            return;
        }

        auto decompStart = std::chrono::steady_clock::now();
        bool decompOk = Decompressor::decompress(firmwarePath, outputDir_, filename, errorMsg);
        auto decompElapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - decompStart).count();

        if (!decompOk) {
            std::cerr << "[Daemon] Decompression failed after " << decompElapsed
                      << "s: " << errorMsg << std::endl;
            fireInstallProgressEvent(versionId, 0, "failed",
                                     "Decompression failed: " + errorMsg);
            return;
        }
        std::cout << "[Daemon] Decompression succeeded in " << decompElapsed << "s" << std::endl;

        if (cancelled_) {
            std::cout << "[Daemon] Install cancelled after decompression" << std::endl;
            fireInstallProgressEvent(versionId, 0, "cancelled", "Install cancelled after decompression");
            return;
        }

        fireInstallProgressEvent(versionId, 80, "verifying", "Decompression complete, verifying...");

        uint64_t outputSize = 0;
        std::string decompressedPath = outputDir_ + "/" +
            (format == "bzip2" ? filename.substr(0, filename.length() - 4) :
             format == "gzip" ? filename.substr(0, filename.length() - 3) : filename);
        std::error_code ec;
        outputSize = std::filesystem::file_size(decompressedPath, ec);
        std::cout << "[Daemon] Decompressed file size: " << outputSize
                  << " bytes (" << (outputSize / 1'048'576.0) << " MB)" << std::endl;

        std::this_thread::sleep_for(500ms);

        fireInstallProgressEvent(versionId, 100, "complete",
                                 "Firmware version 0x" + std::to_string(versionId) + " installed successfully");
        std::cout << "[Daemon] Installation complete for version 0x" << std::hex << versionId
                  << std::dec << " -> " << decompressedPath << std::endl;
    }
};

int main(int argc, char** argv) {
    std::cout << "OTA Daemon (DaemonControl server) starting..." << std::endl;

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    ClientConfig config;
    std::string configPath = (argc > 1) ? argv[1] : "daemon_config.json";
    if (!config.loadFromFile(configPath)) {
        std::cerr << "Failed to load config: " << configPath << std::endl;
        return 1;
    }

    std::string downloadDir = config.getSettings().downloadPath;
    std::string outputDir = config.getSettings().decompressionPath;

    auto runtime = CommonAPI::Runtime::get();
    if (!runtime) {
        std::cerr << "Failed to get CommonAPI Runtime" << std::endl;
        return 1;
    }

    auto service = std::make_shared<DaemonControlImpl>(downloadDir, outputDir);
    std::string domain = "local";
    std::string instance = "manager.updater.DaemonControl";

    bool registered = runtime->registerService(domain, instance, service);
    if (!registered) {
        std::cerr << "Failed to register DaemonControl service" << std::endl;
        return 1;
    }

    std::cout << "DaemonControl service registered." << std::endl;
    std::cout << "  Download dir: " << downloadDir << std::endl;
    std::cout << "  Output dir: " << outputDir << std::endl;
    std::cout << "Waiting for relay commands..." << std::endl;

    unsigned int heartbeat = 0;
    while (g_running) {
        std::this_thread::sleep_for(1s);
        if (++heartbeat % 30 == 0) {
            std::cout << "[Daemon] Heartbeat - waiting for relay commands"
                      << " (downloadDir=" << downloadDir
                      << ", outputDir=" << outputDir << ")" << std::endl;
        }
    }

    std::cout << "Daemon shutting down." << std::endl;
    return 0;
}

