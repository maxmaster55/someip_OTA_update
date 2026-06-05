cmake_minimum_required(VERSION 3.10.0)
project(Update_Manager VERSION 0.1.0 LANGUAGES C CXX)

include_directories(
    inc
    ${CMAKE_CURRENT_SOURCE_DIR}/gen/src-gen
)

find_package(CommonAPI REQUIRED)
find_package(CommonAPI-SomeIP REQUIRED)


set(GENERATED_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/gen/src-gen/v1/manager/updater/UpdaterSomeIPDeployment.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gen/src-gen/v1/manager/updater/UpdaterSomeIPProxy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gen/src-gen/v1/manager/updater/UpdaterSomeIPStubAdapter.cpp
)


add_executable(Update_Manager 
    src/main.cpp
    src/update_manager.cc
    ${GENERATED_SOURCES}
)

target_link_libraries(${PROJECT_NAME}
    vsomeip3
    vsomeip3-cfg
    vsomeip3-e2e
    vsomeip3-sd
    CommonAPI
    CommonAPI-SomeIP
)