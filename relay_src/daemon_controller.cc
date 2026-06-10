#include "daemon_controller.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

DaemonController::DaemonController(const std::string& domain, const std::string& instance)
    : domain_(domain), instance_(instance) {}

bool DaemonController::connect() {
    runtime_ = CommonAPI::Runtime::get();
    if (!runtime_) {
        std::cerr << "[DaemonController] Failed to get CommonAPI Runtime" << std::endl;
        return false;
    }

    for (int i = 0;; ++i) {
        proxy_ = runtime_->buildProxy<v1::manager::updater::DaemonControlProxy>(domain_, instance_);
        if (proxy_ && proxy_->isAvailable()) {
            std::cout << "[DaemonController] Connected to Daemon service" << std::endl;
            return true;
        }
        std::cout << "[DaemonController] Waiting for daemon... (attempt " << (i + 1) << ")" << std::endl;
        std::this_thread::sleep_for(2s);
    }
}

bool DaemonController::isAvailable() const {
    return proxy_ && proxy_->isAvailable();
}

bool DaemonController::triggerInstall(const std::string& firmwarePath, uint32_t versionId,
                                       std::string& outMessage) {
    if (!proxy_) {
        outMessage = "Daemon proxy not available";
        return false;
    }

    if (!proxy_->isAvailable()) {
        std::cerr << "[DaemonController] Proxy exists but not available!" << std::endl;
        outMessage = "Daemon not reachable";
        return false;
    }

    std::cout << "[DaemonController] Calling performInstall: path=" << firmwarePath
              << ", version=0x" << std::hex << versionId << std::dec << std::endl;

    CommonAPI::CallStatus callStatus;
    bool accepted = false;
    std::string message;

    proxy_->performInstall(firmwarePath, versionId, callStatus, accepted, message);
    outMessage = message;

    std::cout << "[DaemonController] performInstall result: callStatus="
              << static_cast<int>(callStatus)
              << ", accepted=" << (accepted ? "yes" : "no")
              << ", message=" << message << std::endl;

    if (callStatus != CommonAPI::CallStatus::SUCCESS) {
        std::cerr << "[DaemonController] performInstall call failed" << std::endl;
        return false;
    }

    return accepted;
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
