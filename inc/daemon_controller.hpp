#pragma once

#include <string>
#include <memory>
#include <functional>
#include <CommonAPI/CommonAPI.hpp>
#include <v1/manager/updater/DaemonControlProxy.hpp>

class DaemonController {
public:
    using ProgressCallback = std::function<void(uint32_t versionId, uint32_t progress,
                                                const std::string& state, const std::string& message)>;

    DaemonController(const std::string& domain, const std::string& instance);
    ~DaemonController() = default;

    bool connect();
    bool isAvailable() const;

    bool triggerInstall(const std::string& firmwarePath, uint32_t versionId,
                        std::string& outMessage);
    bool cancelInstall(std::string& outMessage);

    void subscribeToProgress(ProgressCallback callback);

private:
    std::string domain_;
    std::string instance_;
    std::shared_ptr<CommonAPI::Runtime> runtime_;
    std::shared_ptr<v1::manager::updater::DaemonControlProxy<>> proxy_;
    ProgressCallback progressCallback_;
};
