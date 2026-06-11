cmake_minimum_required(VERSION 3.16)
project(ota_shared LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CommonAPI REQUIRED)
find_package(CommonAPI-SomeIP REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(BZip2 REQUIRED)
find_package(ZLIB REQUIRED)
find_package(nlohmann_json REQUIRED)

set(GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../gen/src-gen/v1/manager/updater)

set(OTA_GENERATED_SOURCES
    ${GEN_DIR}/UpdaterSomeIPDeployment.cpp
    ${GEN_DIR}/UpdaterSomeIPProxy.cpp
    ${GEN_DIR}/UpdaterSomeIPStubAdapter.cpp
    ${GEN_DIR}/RelayControlSomeIPDeployment.cpp
    ${GEN_DIR}/RelayControlSomeIPProxy.cpp
    ${GEN_DIR}/RelayControlSomeIPStubAdapter.cpp
    ${GEN_DIR}/DaemonControlSomeIPDeployment.cpp
    ${GEN_DIR}/DaemonControlSomeIPProxy.cpp
    ${GEN_DIR}/DaemonControlSomeIPStubAdapter.cpp
)

add_library(ota_shared STATIC
    src/file_decompressor.cpp
    src/file_manager.cpp
    src/config_manager.cpp
    ${OTA_GENERATED_SOURCES}
)

target_include_directories(ota_shared PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../gen/src-gen
)

target_link_libraries(ota_shared PUBLIC
    vsomeip3
    vsomeip3-cfg
    vsomeip3-sd
    CommonAPI
    CommonAPI-SomeIP
    OpenSSL::Crypto
    BZip2::BZip2
    ZLIB::ZLIB
    nlohmann_json::nlohmann_json
)
