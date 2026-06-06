#pragma once

#include <v1/manager/updater/UpdaterStubDefault.hpp>
#include <memory>
#include "file_manager.hpp"

class updaterImpl : public v1::manager::updater::UpdaterStubDefault {
public:
    updaterImpl();
    ~updaterImpl() = default;

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

    // Load update file for OTA delivery
    bool loadUpdateFile(const std::string& filename) {
        return updateManager_->loadUpdateFile(filename);
    }

private:
    std::shared_ptr<UpdateManager> updateManager_;
    static constexpr uint32_t CHUNK_SIZE = 4096;
};