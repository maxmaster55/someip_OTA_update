#include "shared/file_manager.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

UpdateManager::UpdateManager(const std::string& basePath)
    : basePath_(basePath) {
    metadata_ = {0, "", 0.0, 0, "", false};
}

bool UpdateManager::parseVersionFromFilename(const std::string& filename, double& version) {
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
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }
    return ss.str();
}

bool UpdateManager::loadUpdateFile(const std::string& filename, const std::string& versionOverride) {
    try {
        double version = 0.0;
        if (!parseVersionFromFilename(filename, version)) {
            if (versionOverride.empty()) {
                std::cerr << "Failed to parse version from filename: " << filename << std::endl;
                return false;
            }
            try {
                version = std::stod(versionOverride);
            } catch (...) {
                std::cerr << "Invalid version override: " << versionOverride << std::endl;
                return false;
            }
            std::cout << "Using version override: " << version << std::endl;
        }

        if (filename[0] == '/') {
            currentFilePath_ = filename;
        } else {
            currentFilePath_ = basePath_ + "/" + filename;
        }

        std::ifstream file(currentFilePath_);
        if (!file.good()) {
            std::cerr << "File not found: " << currentFilePath_ << std::endl;
            return false;
        }
        file.close();

        readFileIntoBuffer();

        metadata_.filename = filename;
        metadata_.version = version;
        metadata_.versionId = versionToId(version);
        metadata_.isCompressed = filename.find(".bz2") != std::string::npos ||
                               filename.find(".gz") != std::string::npos;

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

