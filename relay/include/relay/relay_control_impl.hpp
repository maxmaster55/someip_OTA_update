#pragma once

#include <v1/manager/updater/RelayControlStubDefault.hpp>
#include <memory>
#include <functional>
#include <shared/command_types.hpp>

class RelayControlImpl : public v1::manager::updater::RelayControlStubDefault {
public:
    using CommandHandler = std::function<bool(uint32_t commandCode, uint32_t versionId,
                                              uint32_t parameter, std::string& outMessage)>;
    using VersionQueryHandler = std::function<void(uint32_t& versionId, std::string& versionString)>;

    RelayControlImpl();
    ~RelayControlImpl() = default;

    void setCommandHandler(CommandHandler handler) { commandHandler_ = handler; }
    void setVersionQueryHandler(VersionQueryHandler handler) { versionQueryHandler_ = handler; }

    void sendCommand(const std::shared_ptr<CommonAPI::ClientId> _client,
                     uint32_t _commandCode,
                     uint32_t _versionId,
                     uint32_t _parameter,
                     sendCommandReply_t _reply) override;

    void getCurrentVersion(const std::shared_ptr<CommonAPI::ClientId> _client,
                           getCurrentVersionReply_t _reply) override;

    void notifyStateChanged(const std::string& currentState, uint32_t progress,
                            uint32_t versionId, const std::string& message);

private:
    CommandHandler commandHandler_;
    VersionQueryHandler versionQueryHandler_;
};

