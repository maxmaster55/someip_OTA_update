#pragma once

#include <string>
#include <memory>
#include <functional>
#include <CommonAPI/CommonAPI.hpp>
#include <v1/manager/updater/UpdaterProxy.hpp>
#include <shared/transfer_config.hpp>

class DownloadClient {
public:
    using ProgressCallback = std::function<void(uint32_t versionId, uint32_t progress, const std::string& status)>;
    using CompletionCallback = std::function<void(bool success, const std::string& filePath, const std::string& message)>;

    DownloadClient(const std::string& domain, const std::string& instance);
    ~DownloadClient() = default;

    bool connect();
    void connectAsync();
    bool isAvailable() const;
    bool checkForUpdate(uint32_t& versionId, int64_t& fileSize, std::string& md5Hash, bool& isCompressed);
    bool downloadUpdate(uint32_t versionId, const std::string& downloadPath,
                        int64_t fileSize, const std::string& md5Hash, bool isCompressed,
                        ProgressCallback onProgress = nullptr,
                        CompletionCallback onComplete = nullptr);

    const std::string& getLastFilePath() const { return lastFilePath_; }

private:
    static constexpr int MAX_CONNECT_RETRIES = 30;

    std::string domain_;
    std::string instance_;
    std::shared_ptr<CommonAPI::Runtime> runtime_;
    std::shared_ptr<v1::manager::updater::UpdaterProxy<>> proxy_;

    std::string lastFilePath_;
};

