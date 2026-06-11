#pragma once

#include <v1/manager/updater/UpdaterStubDefault.hpp>
#include <v1/manager/updater/RelayControlProxy.hpp>
#include <memory>
#include <chrono>
#include <atomic>
#include <shared/transfer_config.hpp>
#include <shared/file_manager.hpp>

class updaterImpl : public v1::manager::updater::UpdaterStubDefault {
public:
    updaterImpl();
    ~updaterImpl() = default;

    void setRelayProxy(std::shared_ptr<v1::manager::updater::RelayControlProxy<>> relayProxy) {
        relayProxy_ = relayProxy;
    }

    void getUpdateInfo(const std::shared_ptr<CommonAPI::ClientId> _client, getUpdateInfoReply_t _reply) override;

    void getDownloadStatus(
        const std::shared_ptr<CommonAPI::ClientId> _client,
        uint32_t _versionId,
        bool _success,
        bool _retry,
        std::string _message,
        getDownloadStatusReply_t _reply
    ) override;

    void getInstallationStatus(
        const std::shared_ptr<CommonAPI::ClientId> _client,
        uint32_t _versionId,
        bool _success,
        std::string _message,
        getInstallationStatusReply_t _reply
    ) override;

    void requestData(
        const std::shared_ptr<CommonAPI::ClientId> _client,
        uint32_t _versionId,
        uint32_t _chunkIndex,
        requestDataReply_t _reply
    ) override;

    bool loadUpdateFile(const std::string& filename, const std::string& versionOverride = "") {
        return updateManager_->loadUpdateFile(filename, versionOverride);
    }

    uint32_t getVersionId() const {
        return updateManager_->getMetadata().versionId;
    }

private:
    std::shared_ptr<UpdateManager> updateManager_;
    std::shared_ptr<v1::manager::updater::RelayControlProxy<>> relayProxy_;
    static constexpr uint32_t CHUNK_SIZE = ::CHUNK_SIZE;

    std::chrono::steady_clock::time_point transferStartTime_;
    std::atomic<bool> transferStarted_{false};
};

