#pragma once

#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class ClientConfig {
public:
    struct Settings {
        std::string downloadPath;
        std::string decompressionPath;
        std::string decompressionMode = "post";
        uint32_t chunkSize = 4096;
        uint32_t checkIntervalSec = 60;
        bool autoDecompress = true;
        bool autoCleanup = true;
        std::string serviceDomain = "local";
        std::string serviceInstance = "manager.updater.Updater";
    };

    ClientConfig() = default;
    ~ClientConfig() = default;

    bool loadFromFile(const std::string& configPath);

    const Settings& getSettings() const { return settings_; }

    bool validatePaths() const;

private:
    Settings settings_;

    bool parseJson(const json& jsonObj);
};

