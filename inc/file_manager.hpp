#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <regex>
#include <fstream>
#include <openssl/md5.h>

class UpdateManager {
public:
    struct UpdateMetadata {
        uint32_t versionId;
        std::string filename;
        double version;
        int64_t fileSize;
        std::string md5Hash;
        bool isCompressed;
    };

    UpdateManager(const std::string& basePath = ".");
    ~UpdateManager() = default;

    // Parse version from filename like "file_ota_update_2.5.wic.bz2"
    static bool parseVersionFromFilename(const std::string& filename, double& version);

    // Load update file and generate metadata
    // versionOverride: if set and filename has no parsed version, use this instead
    bool loadUpdateFile(const std::string& filename, const std::string& versionOverride = "");

    // Get current update metadata
    const UpdateMetadata& getMetadata() const { return metadata_; }

    // Get file chunk by index
    std::string getChunk(uint32_t chunkIndex, uint32_t chunkSize = 4096) const;

    // Check if there are more chunks
    bool hasMoreChunks(uint32_t chunkIndex, uint32_t chunkSize = 4096) const;

    // Calculate MD5 hash of file
    static std::string calculateMD5(const std::string& filepath);

private:
    std::string basePath_;
    std::string currentFilePath_;
    UpdateMetadata metadata_;
    std::vector<uint8_t> fileBuffer_;

    uint32_t versionToId(double version);
    void readFileIntoBuffer();
};
