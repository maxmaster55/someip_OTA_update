#include "file_manager.hpp"
#include <iostream>
#include <sstream>
#include <cmath>
#include <algorithm>

UpdateManager::UpdateManager(const std::string& basePath)
    : basePath_(basePath) {
    metadata_ = {0, "", 0.0, 0, "", false};
}

bool UpdateManager::parseVersionFromFilename(const std::string& filename, double& version) {
    // Pattern: file_ota_update_X.Y.wic.bz2 or similar
    // Extract version number (e.g., "2.5" from "file_ota_update_2.5.wic.bz2")
    std::regex versionPattern(R"(\d+\.\d+)");
    std::smatch match;

    if (std::regex_search(filename, match, versionPattern)) {
        try {
            version = std::stod(match.str());
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse version: " << e.what() << std::endl;
            return false;
        }
    }
    return false;
}

uint32_t UpdateManager::versionToId(double version) {
    // Convert version like 2.5 to version ID
    // e.g., 2.5 -> 0x0205 (2 * 100 + 5)
    uint32_t major = static_cast<uint32_t>(version);
    uint32_t minor = static_cast<uint32_t>((version - major) * 100);
    return (major << 16) | minor;
}

void UpdateManager::readFileIntoBuffer() {
    std::ifstream file(currentFilePath_, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + currentFilePath_);
    }

    metadata_.fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    fileBuffer_.resize(metadata_.fileSize);
    file.read(reinterpret_cast<char*>(fileBuffer_.data()), metadata_.fileSize);

    if (!file) {
        throw std::runtime_error("Failed to read file completely");
    }
    file.close();
}

std::string UpdateManager::calculateMD5(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }

    MD5_CTX md5Context;
    MD5_Init(&md5Context);

    const size_t bufferSize = 4096;
    char buffer[bufferSize];

    while (file.good()) {
        file.read(buffer, bufferSize);
        MD5_Update(&md5Context, buffer, file.gcount());
    }
    file.close();

    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5_Final(digest, &md5Context);

    std::stringstream ss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        ss << std::hex << static_cast<int>(digest[i]);
    }
    return ss.str();
}

bool UpdateManager::loadUpdateFile(const std::string& filename) {
    try {
        // Parse version from filename
        double version = 0.0;
        if (!parseVersionFromFilename(filename, version)) {
            std::cerr << "Failed to parse version from filename: " << filename << std::endl;
            return false;
        }

        // Use absolute path if provided, otherwise prepend basePath
        if (filename[0] == '/') {
            currentFilePath_ = filename;
        } else {
            currentFilePath_ = basePath_ + "/" + filename;
        }

        // Check if file exists
        std::ifstream file(currentFilePath_);
        if (!file.good()) {
            std::cerr << "File not found: " << currentFilePath_ << std::endl;
            return false;
        }
        file.close();

        // Read file into buffer
        readFileIntoBuffer();

        // Calculate metadata
        metadata_.filename = filename;
        metadata_.version = version;
        metadata_.versionId = versionToId(version);
        metadata_.isCompressed = filename.find(".bz2") != std::string::npos ||
                               filename.find(".gz") != std::string::npos;

        // Calculate MD5 hash
        metadata_.md5Hash = calculateMD5(currentFilePath_);

        std::cout << "Successfully loaded update file:" << std::endl;
        std::cout << "  Filename: " << filename << std::endl;
        std::cout << "  Version: " << version << std::endl;
        std::cout << "  Size: " << metadata_.fileSize << " bytes" << std::endl;
        std::cout << "  MD5: " << metadata_.md5Hash << std::endl;
        std::cout << "  Compressed: " << (metadata_.isCompressed ? "yes" : "no") << std::endl;

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading update file: " << e.what() << std::endl;
        return false;
    }
}

std::string UpdateManager::getChunk(uint32_t chunkIndex, uint32_t chunkSize) const {
    if (fileBuffer_.empty()) {
        return "";
    }

    size_t startPos = static_cast<size_t>(chunkIndex) * chunkSize;
    if (startPos >= fileBuffer_.size()) {
        return "";
    }

    size_t endPos = std::min(startPos + chunkSize, fileBuffer_.size());
    size_t length = endPos - startPos;

    return std::string(reinterpret_cast<const char*>(fileBuffer_.data() + startPos), length);
}

bool UpdateManager::hasMoreChunks(uint32_t chunkIndex, uint32_t chunkSize) const {
    size_t startPos = (static_cast<size_t>(chunkIndex) + 1) * chunkSize;
    return startPos < fileBuffer_.size();
}
