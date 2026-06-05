#include <iostream>
#include "CommonAPI/CommonAPI.hpp"
#include "updater_impl.hpp"
#include <thread>
#include <chrono>

using namespace std::chrono_literals;



int main(int, char**){
    std::cout << "Update_Manager is running!\n";
    auto runtime = CommonAPI::Runtime::get();
    
    if(!runtime){
        std::cerr << "Failed to get CommonAPI Runtime instance\n";
        return 1;
    }

    std::string domain = "local";
    std::string instance = "manager.updater.Updater";

    auto service = std::make_shared<updaterImpl>();
    bool registered = runtime->registerService(domain, instance, service);

    if (!registered) {
        std::cerr << "Failed to register service\n";
        return 1;
    }
    std::cout << "Service registered successfully." << std::endl;

    while (true)
        std::this_thread::sleep_for(1s);


}
