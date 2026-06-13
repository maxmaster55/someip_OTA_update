cmake_minimum_required(VERSION 3.16)
project(ota_gui LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CommonAPI REQUIRED)
find_package(CommonAPI-SomeIP REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2)

qt_standard_project_setup(REQUIRES 6.8)

set(SHARED_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
set(GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../gen/src-gen/v1/manager/updater)

qt_add_executable(ota_gui
    src/main.cpp
    src/download_manager.cpp
    include/gui/download_manager.hpp
    ${SHARED_SRC}/src/file_manager.cpp
    ${GEN_DIR}/UpdaterSomeIPDeployment.cpp
    ${GEN_DIR}/UpdaterSomeIPStubAdapter.cpp
    ${GEN_DIR}/RelayControlSomeIPDeployment.cpp
    ${GEN_DIR}/RelayControlSomeIPProxy.cpp
)

qt_add_qml_module(ota_gui
    URI OtaGui
    QML_FILES
        qml/Main.qml
)

target_include_directories(ota_gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SHARED_SRC}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../gen/src-gen
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
