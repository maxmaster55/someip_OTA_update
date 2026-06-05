#include <updater_impl.hpp>


void updaterImpl::getUpdateInfo(const std::shared_ptr<CommonAPI::ClientId> _client, getUpdateInfoReply_t _reply) 
{

}

void updaterImpl::sendDownloadStatus(
    const std::shared_ptr<CommonAPI::ClientId> _client, 
    bool _success, 
    bool _retry, 
    std::string _message, 
    sendDownloadStatusReply_t _reply
) 
{

}

void updaterImpl::sendInstallationStatus(
    const std::shared_ptr<CommonAPI::ClientId> _client, 
    bool _success, 
    std::string _message, 
    sendInstallationStatusReply_t _reply
) 
{

}

void updaterImpl::startUpdate(const std::shared_ptr<CommonAPI::ClientId> _client, startUpdateReply_t _reply) 
{

}
