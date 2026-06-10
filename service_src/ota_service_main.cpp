#include <iostream>
#include "CommonAPI/CommonAPI.hpp"
#include "ota_updater_impl.hpp"
#include <thread>
#include <chrono>

using namespace std::chrono_literals;





int main(int argc, char** argv) {
    // Check for update file argument
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <update_file> [version_override]" << std::endl;
        std::cerr << "Example: " << argv[0] << " file_ota_update_2.5.wic.bz2" << std::endl;
        std::cerr << "         " << argv[0] << " custom_firmware.bin 3.1" << std::endl;
        return 1;
    }

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

    // Load the update file
    if (!service->loadUpdateFile(updateFile, versionOverride)) {
        std::cerr << "Failed to load update file: " << updateFile << std::endl;
        return 1;
    }

    bool registered = runtime->registerService(domain, instance, service);

    if (!registered) {
        std::cerr << "Failed to register service\n";
        return 1;
    }

    std::cout << "Service registered successfully. Waiting for client requests..." << std::endl;

    while (true)
        std::this_thread::sleep_for(1s);

    return 0;
}
