cmake_minimum_required(VERSION 3.16)
project(ota_daemon LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CommonAPI REQUIRED)
find_package(CommonAPI-SomeIP REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(BZip2 REQUIRED)
find_package(ZLIB REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(LibArchive REQUIRED)

set(SHARED_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
set(GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../gen/src-gen/v1/manager/updater)

add_executable(ota_daemon
    src/ota_daemon_main.cpp
    ${SHARED_SRC}/src/file_decompressor.cpp
    ${SHARED_SRC}/src/config_manager.cpp
    ${GEN_DIR}/DaemonControlSomeIPDeployment.cpp
    ${GEN_DIR}/DaemonControlSomeIPProxy.cpp
    ${GEN_DIR}/DaemonControlSomeIPStubAdapter.cpp
)

target_include_directories(ota_daemon PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SHARED_SRC}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../gen/src-gen
)

target_link_libraries(ota_daemon PRIVATE
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
    ${LibArchive_LIBRARY}
)
target_include_directories(ota_daemon PRIVATE ${LibArchive_INCLUDE_DIR})
