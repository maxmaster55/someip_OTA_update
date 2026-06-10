#include "relay_control_impl.hpp"
#include <iostream>

RelayControlImpl::RelayControlImpl() {}

void RelayControlImpl::sendCommand(const std::shared_ptr<CommonAPI::ClientId> _client,
                                    uint32_t _commandCode,
                                    uint32_t _versionId,
                                    uint32_t _parameter,
                                    sendCommandReply_t _reply) {
    (void)_client;

    std::cout << "[RelayControl] Command received: code=" << _commandCode
              << ", version=0x" << std::hex << _versionId
              << std::dec << ", param=" << _parameter << std::endl;

    if (commandHandler_) {
        std::string message;
        bool accepted = commandHandler_(_commandCode, _versionId, _parameter, message);
        _reply(accepted, message);
    } else {
        _reply(false, "No command handler registered");
    }
}

void RelayControlImpl::getCurrentVersion(const std::shared_ptr<CommonAPI::ClientId> _client,
                                          getCurrentVersionReply_t _reply) {
    (void)_client;

    if (versionQueryHandler_) {
        uint32_t versionId = 0;
        std::string versionString = "0.0";
        versionQueryHandler_(versionId, versionString);
        _reply(versionId, versionString);
    } else {
        _reply(0, "0.0");
    }
}

void RelayControlImpl::notifyStateChanged(const std::string& currentState, uint32_t progress,
                                           uint32_t versionId, const std::string& message) {
    fireStateChangedEvent(currentState, progress, versionId, message);
}
