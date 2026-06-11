#pragma once

#include <cstdint>
#include <string>

inline constexpr uint32_t CHUNK_SIZE = 1387;
inline constexpr uint32_t WINDOW_SIZE = 128;

struct RelayConfig {
    std::string downloadPath = "/tmp/ota_downloads";
    std::string decompressionPath = "/tmp/ota_extracted";
    uint32_t checkIntervalSec = 30;
    bool autoCleanup = true;
    bool autoInstall = false;

    std::string serviceDomain = "local";
    std::string serviceInstance = "manager.updater.Updater";

    std::string relayDomain = "local";
    std::string relayInstance = "manager.updater.RelayControl";

    std::string daemonDomain = "local";
    std::string daemonInstance = "manager.updater.DaemonControl";
};

