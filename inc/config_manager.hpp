#pragma once

#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class ClientConfig {
public:
    struct Settings {
        std::string downloadPath;      // Path to save downloaded file
        std::string decompressionPath; // Path to decompress file into
        uint32_t chunkSize = 4096;     // Chunk size for download
        uint32_t checkIntervalSec = 60; // How often to check for updates
        bool autoDecompress = true;    // Automatically decompress after download
        bool autoCleanup = true;       // Delete compressed file after decompress
        std::string serviceDomain = "local";
        std::string serviceInstance = "manager.updater.Updater";
    };

    ClientConfig() = default;
    ~ClientConfig() = default;

    // Load configuration from JSON file
    bool loadFromFile(const std::string& configPath);

    // Get settings
    const Settings& getSettings() const { return settings_; }

    // Validate configuration paths
    bool validatePaths() const;

private:
    Settings settings_;

    // Parse JSON object into Settings
    bool parseJson(const json& jsonObj);
};

