#pragma once

#include <v1/manager/updater/UpdaterStubDefault.hpp>


class updaterImpl : public v1::manager::updater::UpdaterStubDefault {
public:
    updaterImpl() = default;
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
private:
    
};