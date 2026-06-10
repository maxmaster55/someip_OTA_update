cmake_minimum_required(VERSION 3.16)
project(OTA_UpdateSystem VERSION 0.1.0 LANGUAGES C CXX)

find_package(CommonAPI REQUIRED)
find_package(CommonAPI-SomeIP REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(BZip2 REQUIRED)
find_package(ZLIB REQUIRED)
find_package(nlohmann_json REQUIRED)

set(GENERATED_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/gen/src-gen/v1/manager/updater/UpdaterSomeIPDeployment.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gen/src-gen/v1/manager/updater/UpdaterSomeIPProxy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gen/src-gen/v1/manager/updater/UpdaterSomeIPStubAdapter.cpp
)

# OTA Service - Provides firmware files
add_executable(ota_service
    service_src/ota_service_main.cpp
    service_src/ota_updater_impl.cc
    service_src/file_manager.cc
    ${GENERATED_SOURCES}
)

target_include_directories(ota_service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/gen/src-gen
    inc
)

target_link_libraries(ota_service
    vsomeip3
    vsomeip3-cfg
    vsomeip3-e2e
    vsomeip3-sd
    CommonAPI
    CommonAPI-SomeIP
    OpenSSL::Crypto
)

# OTA Daemon - Downloads and decompresses updates
add_executable(ota_daemon
    daemon_src/ota_daemon_main.cpp
    daemon_src/config_manager.cc
    daemon_src/file_decompressor.cc
    ${GENERATED_SOURCES}
)

target_include_directories(ota_daemon PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/gen/src-gen
    inc
)

target_link_libraries(ota_daemon
    vsomeip3
    vsomeip3-cfg
    vsomeip3-e2e
    vsomeip3-sd
    CommonAPI
    CommonAPI-SomeIP
    OpenSSL::Crypto
    BZip2::BZip2
    ZLIB::ZLIB
    nlohmann_json::nlohmann_json
)

# OTA GUI - Graphical interface for update management
find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.8)

set(GUI_GENERATED_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/gen/src-gen/v1/manager/updater/UpdaterSomeIPProxy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gen/src-gen/v1/manager/updater/UpdaterSomeIPDeployment.cpp
)

qt_add_executable(ota_gui
    gui_src/main.cpp
    gui_src/download_manager.cpp
    ${GUI_GENERATED_SOURCES}
)

qt_add_qml_module(ota_gui
    URI OtaGui
    QML_FILES
        qml/Main.qml
)

target_include_directories(ota_gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/gen/src-gen
    inc
)

target_link_libraries(ota_gui
    PRIVATE Qt6::Quick
    PRIVATE Qt6::QuickControls2
    PRIVATE vsomeip3
    PRIVATE vsomeip3-sd
    PRIVATE CommonAPI
    PRIVATE CommonAPI-SomeIP
    PRIVATE OpenSSL::Crypto
)

install(TARGETS ota_daemon ota_service ota_gui
    RUNTIME DESTINATION ${bindir}
)

