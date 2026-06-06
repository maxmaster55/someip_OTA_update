#include <updater_impl.hpp>


void updaterImpl::getUpdateInfo(const std::shared_ptr<CommonAPI::ClientId> _client, getUpdateInfoReply_t _reply) 
{

}

void updaterImpl::getDownloadStatus(
    const std::shared_ptr<CommonAPI::ClientId> _client, 
    uint32_t _versionId,
    bool _success, 
    bool _retry, 
    std::string _message, 
    getDownloadStatusReply_t _reply
) 
{

}

void updaterImpl::getInstallationStatus(
    const std::shared_ptr<CommonAPI::ClientId> _client, 
    uint32_t _versionId,
    bool _success, 
    std::string _message, 
    getInstallationStatusReply_t _reply
) 
{

}

void updaterImpl::requestData(
    const std::shared_ptr<CommonAPI::ClientId> _client, 
    uint32_t _versionId, 
    uint32_t _chunkIndex, 
    requestDataReply_t _reply
) 
{

}
