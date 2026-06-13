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

    static bool parseVersionFromFilename(const std::string& filename, double& version);

    bool loadUpdateFile(const std::string& filename, const std::string& versionOverride = "");

    const UpdateMetadata& getMetadata() const { return metadata_; }

    std::string getChunk(uint32_t chunkIndex, uint32_t chunkSize = 4096) const;

    bool hasMoreChunks(uint32_t chunkIndex, uint32_t chunkSize = 4096) const;

    static std::string calculateMD5(const std::string& filepath);

private:
    std::string basePath_;
    std::string currentFilePath_;
    UpdateMetadata metadata_;
    std::vector<uint8_t> fileBuffer_;

    uint32_t versionToId(double version);
    void readFileIntoBuffer();
};

