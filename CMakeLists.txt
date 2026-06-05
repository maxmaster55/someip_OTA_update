cmake_minimum_required(VERSION 3.10.0)
project(Update_Manager VERSION 0.1.0 LANGUAGES C CXX)



find_package(CommonAPI REQUIRED)
find_package(CommonAPI-SomeIP REQUIRED)


set(GENERATED_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/gen/src-gen/v1/manager/updater/UpdaterSomeIPDeployment.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gen/src-gen/v1/manager/updater/UpdaterSomeIPProxy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gen/src-gen/v1/manager/updater/UpdaterSomeIPStubAdapter.cpp
)

# service
add_executable(Update_Notifier 
    service_src/main.cpp
    service_src/updater_impl.cc
    ${GENERATED_SOURCES}
)

target_include_directories(Update_Notifier PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/gen/src-gen
    inc
)

target_link_libraries(Update_Notifier
    vsomeip3
    vsomeip3-cfg
    vsomeip3-e2e
    vsomeip3-sd
    CommonAPI
    CommonAPI-SomeIP
)

# client
add_executable(Update_Manager
    client_src/main.cpp
    ${GENERATED_SOURCES}
)

target_include_directories(Update_Manager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/gen/src-gen
    inc
)


target_link_libraries(Update_Manager
    vsomeip3
    vsomeip3-cfg
    vsomeip3-e2e
    vsomeip3-sd
    CommonAPI
    CommonAPI-SomeIP
)