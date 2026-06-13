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
    void connectAsync();
    bool isAvailable() const;

    bool sendFile(const std::string& firmwarePath, uint32_t versionId,
                  uint64_t fileSize, const std::string& md5Hash, bool isCompressed,
                  std::string& outMessage);

    bool cancelInstall(std::string& outMessage);
    bool triggerInstall(std::string& outMessage);

    void subscribeToProgress(ProgressCallback callback);

private:
    static constexpr int MAX_CONNECT_RETRIES = 30;

    std::string domain_;
    std::string instance_;
    std::shared_ptr<CommonAPI::Runtime> runtime_;
    std::shared_ptr<v1::manager::updater::DaemonControlProxy<>> proxy_;
    ProgressCallback progressCallback_;
};

