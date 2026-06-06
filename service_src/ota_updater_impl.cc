#include "ota_updater_impl.hpp"
#include <iostream>

updaterImpl::updaterImpl() 
    : updateManager_(std::make_shared<UpdateManager>()) {
}

void updaterImpl::getUpdateInfo(const std::shared_ptr<CommonAPI::ClientId> _client, getUpdateInfoReply_t _reply) 
{
    (void)_client;
    const auto& metadata = updateManager_->getMetadata();
    
    std::cout << "getUpdateInfo called - Version: " << metadata.version << ", Size: " << metadata.fileSize << std::endl;
    
    _reply(metadata.versionId, metadata.fileSize, metadata.md5Hash, metadata.isCompressed);
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
    (void)_client;
    (void)_versionId;
    (void)_retry;
    
    std::string status = _success ? "SUCCESS" : "FAILED";
    std::cout << "Download Status - " << status << ": " << _message << std::endl;
    
    _reply();
}

void updaterImpl::getInstallationStatus(
    const std::shared_ptr<CommonAPI::ClientId> _client, 
    uint32_t _versionId,
    bool _success, 
    std::string _message, 
    getInstallationStatusReply_t _reply
) 
{
    (void)_client;
    (void)_versionId;
    
    std::string status = _success ? "SUCCESS" : "FAILED";
    std::cout << "Installation Status - " << status << ": " << _message << std::endl;
    
    _reply();
}

void updaterImpl::requestData(
    const std::shared_ptr<CommonAPI::ClientId> _client, 
    uint32_t _versionId, 
    uint32_t _chunkIndex, 
    requestDataReply_t _reply
) 
{
    (void)_client;
    (void)_versionId;
    
    const auto& metadata = updateManager_->getMetadata();
    
    // Get chunk from the update file
    std::string chunkData = updateManager_->getChunk(_chunkIndex, CHUNK_SIZE);
    bool isLastChunk = !updateManager_->hasMoreChunks(_chunkIndex, CHUNK_SIZE);
    
    std::cout << "Requested chunk " << _chunkIndex << " - Size: " << chunkData.size() 
              << " bytes, Last: " << (isLastChunk ? "yes" : "no") << std::endl;
    
    _reply(_chunkIndex, chunkData, isLastChunk);
}

