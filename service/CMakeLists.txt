cmake_minimum_required(VERSION 3.16)
project(ota_service LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CommonAPI REQUIRED)
find_package(CommonAPI-SomeIP REQUIRED)
find_package(OpenSSL REQUIRED)

set(SHARED_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
set(GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../gen/src-gen/v1/manager/updater)

add_executable(ota_service
    src/ota_service_main.cpp
    src/ota_updater_impl.cpp
    ${SHARED_SRC}/src/file_manager.cpp
    ${GEN_DIR}/UpdaterSomeIPDeployment.cpp
    ${GEN_DIR}/UpdaterSomeIPProxy.cpp
    ${GEN_DIR}/UpdaterSomeIPStubAdapter.cpp
    ${GEN_DIR}/RelayControlSomeIPDeployment.cpp
    ${GEN_DIR}/RelayControlSomeIPProxy.cpp
)

target_include_directories(ota_service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SHARED_SRC}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../gen/src-gen
)

target_link_libraries(ota_service PRIVATE
    vsomeip3
    vsomeip3-cfg
    vsomeip3-e2e
    vsomeip3-sd
    CommonAPI
    CommonAPI-SomeIP
    OpenSSL::Crypto
)
