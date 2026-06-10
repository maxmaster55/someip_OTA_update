#include <iostream>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include <filesystem>
#include "CommonAPI/CommonAPI.hpp"
#include <v1/manager/updater/DaemonControlStubDefault.hpp>
#include "file_decompressor.hpp"
#include "config_manager.hpp"

using namespace std::chrono_literals;

class DaemonControlImpl : public v1::manager::updater::DaemonControlStubDefault {
public:
    explicit DaemonControlImpl(const std::string& outputDir)
        : cancelled_(false), outputDir_(outputDir) {}

    void performInstall(const std::shared_ptr<CommonAPI::ClientId> _client,
                        std::string _firmwarePath,
                        uint32_t _versionId,
                        performInstallReply_t _reply) override {
        (void)_client;
        std::cout << "[Daemon] performInstall called: path=" << _firmwarePath
                  << ", version=0x" << std::hex << _versionId << std::dec << std::endl;

        if (!std::filesystem::exists(_firmwarePath)) {
            std::cerr << "[Daemon] Firmware file not found: " << _firmwarePath << std::endl;
            _reply(false, "File not found");
            return;
        }

        cancelled_ = false;

        std::thread([this, _firmwarePath, _versionId]() {
            doInstall(_firmwarePath, _versionId);
        }).detach();

        _reply(true, "Installation started");
    }

    void cancelInstall(const std::shared_ptr<CommonAPI::ClientId> _client,
                       cancelInstallReply_t _reply) override {
        (void)_client;
        std::cout << "[Daemon] cancelInstall called" << std::endl;
        cancelled_ = true;
        _reply(true);
    }

private:
    std::atomic<bool> cancelled_;
    std::string outputDir_;

    void doInstall(const std::string& firmwarePath, uint32_t versionId) {
        std::cout << "[Daemon] Installing: " << firmwarePath << std::endl;

        fireInstallProgressEvent(versionId, 0, "decompressing", "Starting decompression");

        if (cancelled_) {
            fireInstallProgressEvent(versionId, 0, "cancelled", "Install cancelled before start");
            return;
        }

        std::string format;
        if (!Decompressor::detectFormat(firmwarePath, format)) {
            fireInstallProgressEvent(versionId, 100, "complete",
                                     "File is not compressed, install complete");
            std::cout << "[Daemon] File not compressed, done" << std::endl;
            return;
        }

        std::filesystem::create_directories(outputDir_);

        std::string filename = std::filesystem::path(firmwarePath).filename().string();
        std::string errorMsg;

        fireInstallProgressEvent(versionId, 30, "decompressing",
                                 "Decompressing " + format + " file...");

        if (cancelled_) {
            fireInstallProgressEvent(versionId, 0, "cancelled", "Install cancelled during decompression");
            return;
        }

        if (!Decompressor::decompress(firmwarePath, outputDir_, filename, errorMsg)) {
            std::cerr << "[Daemon] Decompression failed: " << errorMsg << std::endl;
            fireInstallProgressEvent(versionId, 0, "failed",
                                     "Decompression failed: " + errorMsg);
            return;
        }

        if (cancelled_) {
            fireInstallProgressEvent(versionId, 0, "cancelled", "Install cancelled after decompression");
            return;
        }

        fireInstallProgressEvent(versionId, 80, "verifying", "Decompression complete, verifying...");

        std::this_thread::sleep_for(500ms);

        fireInstallProgressEvent(versionId, 100, "complete",
                                 "Firmware version 0x" + std::to_string(versionId) + " installed successfully");
        std::cout << "[Daemon] Installation complete for version 0x" << std::hex << versionId << std::dec << std::endl;
    }
};

int main(int argc, char** argv) {
    std::cout << "OTA Daemon (DaemonControl server) starting..." << std::endl;

    ClientConfig config;
    std::string configPath = (argc > 1) ? argv[1] : "daemon_config.json";
    if (!config.loadFromFile(configPath)) {
        std::cerr << "Failed to load config: " << configPath << std::endl;
        return 1;
    }

    std::string outputDir = config.getSettings().decompressionPath;

    auto runtime = CommonAPI::Runtime::get();
    if (!runtime) {
        std::cerr << "Failed to get CommonAPI Runtime" << std::endl;
        return 1;
    }

    auto service = std::make_shared<DaemonControlImpl>(outputDir);
    std::string domain = "local";
    std::string instance = "manager.updater.DaemonControl";

    bool registered = runtime->registerService(domain, instance, service);
    if (!registered) {
        std::cerr << "Failed to register DaemonControl service" << std::endl;
        return 1;
    }

    std::cout << "DaemonControl service registered. Output dir: " << outputDir << std::endl;
    std::cout << "Waiting for relay commands..." << std::endl;

    while (true) {
        std::this_thread::sleep_for(1s);
    }

    return 0;
}
