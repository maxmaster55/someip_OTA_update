#include "shared/config_manager.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>

bool ClientConfig::loadFromFile(const std::string& configPath) {
    try {
        std::ifstream file(configPath);
        if (!file.is_open()) {
            std::cerr << "Failed to open config file: " << configPath << std::endl;
            return false;
        }

        json jsonObj = json::parse(file);
        file.close();

        return parseJson(jsonObj);
    } catch (const std::exception& e) {
        std::cerr << "Error loading config: " << e.what() << std::endl;
        return false;
    }
}

bool ClientConfig::parseJson(const json& jsonObj) {
    try {
        if (!jsonObj.contains("downloadPath")) {
            std::cerr << "Missing required field: downloadPath" << std::endl;
            return false;
        }

        if (!jsonObj.contains("decompressionPath")) {
            std::cerr << "Missing required field: decompressionPath" << std::endl;
            return false;
        }

        settings_.downloadPath = jsonObj.at("downloadPath").get<std::string>();
        settings_.decompressionPath = jsonObj.at("decompressionPath").get<std::string>();

        if (jsonObj.contains("decompressionMode")) {
            settings_.decompressionMode = jsonObj.at("decompressionMode").get<std::string>();
        }

        if (jsonObj.contains("chunkSize")) {
            settings_.chunkSize = jsonObj.at("chunkSize").get<uint32_t>();
        }

        if (jsonObj.contains("checkIntervalSec")) {
            settings_.checkIntervalSec = jsonObj.at("checkIntervalSec").get<uint32_t>();
        }

        if (jsonObj.contains("autoDecompress")) {
            settings_.autoDecompress = jsonObj.at("autoDecompress").get<bool>();
        }

        if (jsonObj.contains("autoCleanup")) {
            settings_.autoCleanup = jsonObj.at("autoCleanup").get<bool>();
        }

        if (jsonObj.contains("serviceDomain")) {
            settings_.serviceDomain = jsonObj.at("serviceDomain").get<std::string>();
        }

        if (jsonObj.contains("serviceInstance")) {
            settings_.serviceInstance = jsonObj.at("serviceInstance").get<std::string>();
        }

        std::cout << "Configuration loaded successfully:" << std::endl;
        std::cout << "  Download Path: " << settings_.downloadPath << std::endl;
        std::cout << "  Decompression Path: " << settings_.decompressionPath << std::endl;
        std::cout << "  Chunk Size: " << settings_.chunkSize << std::endl;
        std::cout << "  Check Interval: " << settings_.checkIntervalSec << "s" << std::endl;
        std::cout << "  Decompression Mode: " << settings_.decompressionMode << std::endl;
        std::cout << "  Auto Decompress: " << (settings_.autoDecompress ? "yes" : "no") << std::endl;
        std::cout << "  Auto Cleanup: " << (settings_.autoCleanup ? "yes" : "no") << std::endl;

        return validatePaths();
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON: " << e.what() << std::endl;
        return false;
    }
}

bool ClientConfig::validatePaths() const {
    namespace fs = std::filesystem;

    try {
        fs::create_directories(settings_.downloadPath);
        std::cout << "Download path ready: " << settings_.downloadPath << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Cannot create download path: " << e.what() << std::endl;
        return false;
    }

    try {
        fs::create_directories(settings_.decompressionPath);
        std::cout << "Decompression path ready: " << settings_.decompressionPath << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Cannot create decompression path: " << e.what() << std::endl;
        return false;
    }

    return true;
}

