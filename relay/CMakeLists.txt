cmake_minimum_required(VERSION 3.16)
project(ota_relay LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CommonAPI REQUIRED)
find_package(CommonAPI-SomeIP REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)

set(SHARED_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
set(GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../gen/src-gen/v1/manager/updater)

add_executable(ota_relay
    src/ota_relay_main.cpp
    src/download_client.cpp
    src/daemon_controller.cpp
    src/relay_control_impl.cpp
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

target_include_directories(ota_relay PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SHARED_SRC}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../gen/src-gen
)

target_link_libraries(ota_relay PRIVATE
    vsomeip3
    vsomeip3-cfg
    vsomeip3-e2e
    vsomeip3-sd
    CommonAPI
    CommonAPI-SomeIP
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)
