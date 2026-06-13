#include "service/ota_updater_impl.hpp"
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

    if (!transferStarted_.exchange(true)) {
        transferStartTime_ = std::chrono::steady_clock::now();

        const auto& metadata = updateManager_->getMetadata();
        uint32_t totalChunks = (metadata.fileSize + CHUNK_SIZE - 1) / CHUNK_SIZE;
        std::cout << "Serving " << metadata.fileSize << " bytes in "
                  << totalChunks << " chunks"
                  << std::endl;
    }

    std::string chunkData = updateManager_->getChunk(_chunkIndex, CHUNK_SIZE);
    bool isLastChunk = !updateManager_->hasMoreChunks(_chunkIndex, CHUNK_SIZE);

    if (isLastChunk) {
        auto t1 = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(t1 - transferStartTime_).count();
        double speedMbps = (updateManager_->getMetadata().fileSize * 8.0 / 1'000'000.0) / elapsed;
        std::cout << "Transfer done in " << elapsed << " s, " << speedMbps << " Mbps ("
                  << (updateManager_->getMetadata().fileSize / 1'048'576.0 / elapsed) << " MB/s)"
                  << std::endl;
    }

    _reply(_chunkIndex, chunkData, isLastChunk);
}

