cmake_minimum_required(VERSION 3.16)
project(OTA_UpdateSystem VERSION 0.2.0 LANGUAGES C CXX)

# Each sub-project is fully self-contained with its own CMakeLists.txt.
# You can also build any component standalone:
#   cd service && mkdir build && cd build && cmake .. && make

add_subdirectory(service)
add_subdirectory(relay)
add_subdirectory(daemon)

find_package(Qt6 QUIET COMPONENTS Quick QuickControls2)
if(Qt6_FOUND)
    add_subdirectory(gui)
else()
    message(STATUS "Qt6 not found - skipping GUI build")
endif()

install(TARGETS ota_service ota_relay ota_daemon
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(TARGET ota_gui)
    install(TARGETS ota_gui
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()
