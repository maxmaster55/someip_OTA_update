#pragma once

#include <cstdint>

namespace relay {

enum CommandCode : uint32_t {
    UPDATE_NOW = 0,
    UPDATE_SCHEDULED = 1,
    GET_VERSION = 2,
    CANCEL = 3,
    GET_STATUS = 4,
    INSTALL = 5
};

enum class RelayState : uint32_t {
    IDLE = 0,
    DOWNLOADING = 1,
    SCHEDULED = 2,
    READY = 3,
    INSTALLING = 4,
    COMPLETE = 5,
    ERROR = 6
};

inline const char* relayStateToString(RelayState s) {
    switch (s) {
        case RelayState::IDLE: return "idle";
        case RelayState::DOWNLOADING: return "downloading";
        case RelayState::SCHEDULED: return "scheduled";
        case RelayState::READY: return "ready";
        case RelayState::INSTALLING: return "installing";
        case RelayState::COMPLETE: return "complete";
        case RelayState::ERROR: return "error";
        default: return "unknown";
    }
}

}
