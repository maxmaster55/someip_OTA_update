#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include "CommonAPI/CommonAPI.hpp"
#include "service/ota_updater_impl.hpp"
#include <v1/manager/updater/RelayControlProxy.hpp>
#include <shared/command_types.hpp>

using namespace std::chrono_literals;

static std::atomic<bool> g_running{true};

static void signalHandler(int) {
    g_running = false;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <update_file> [version_override]" << std::endl;
        std::cerr << "Example: " << argv[0] << " file_ota_update_2.5.wic.bz2" << std::endl;
        std::cerr << "         " << argv[0] << " custom_firmware.bin 3.1" << std::endl;
        return 1;
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::string updateFile = argv[1];
    std::string versionOverride = (argc >= 3) ? argv[2] : "";
    std::cout << "Update_Notifier is running with file: " << updateFile << std::endl;

    auto runtime = CommonAPI::Runtime::get();

    if (!runtime) {
        std::cerr << "Failed to get CommonAPI Runtime instance\n";
        return 1;
    }

    std::string domain = "local";
    std::string instance = "manager.updater.Updater";

    auto service = std::make_shared<updaterImpl>();

    if (!service->loadUpdateFile(updateFile, versionOverride)) {
        std::cerr << "Failed to load update file: " << updateFile << std::endl;
        return 1;
    }

    bool registered = runtime->registerService(domain, instance, service);

    if (!registered) {
        std::cerr << "Failed to register service\n";
        return 1;
    }

    std::cout << "Service registered successfully." << std::endl;

    std::cout << "Connecting to RelayControl service..." << std::endl;
    std::shared_ptr<v1::manager::updater::RelayControlProxy<>> relayProxy;
    for (int i = 0; i < 15; ++i) {
        relayProxy = runtime->buildProxy<v1::manager::updater::RelayControlProxy>(
            "local", "manager.updater.RelayControl");
        if (relayProxy && relayProxy->isAvailable()) {
            std::cout << "Connected to RelayControl service" << std::endl;
            break;
        }
        std::cout << "Waiting for relay... (attempt " << (i + 1) << ")" << std::endl;
        std::this_thread::sleep_for(2s);
    }

    if (relayProxy && relayProxy->isAvailable()) {
        service->setRelayProxy(relayProxy);
        std::cout << "Relay proxy set on service" << std::endl;

        CommonAPI::CallStatus callStatus;
        bool accepted = false;
        std::string message;
        relayProxy->sendCommand(
            static_cast<uint32_t>(relay::UPDATE_NOW),
            service->getVersionId(),
            0,
            callStatus, accepted, message);
        std::cout << "Sent UPDATE_NOW to relay: accepted=" << accepted
                  << ", message=" << message << std::endl;
    } else {
        std::cout << "Relay not available, running standalone" << std::endl;
    }

    std::cout << "Waiting for client requests..." << std::endl;

    while (g_running)
        std::this_thread::sleep_for(1s);

    std::cout << "Service shutting down." << std::endl;
    return 0;
}

