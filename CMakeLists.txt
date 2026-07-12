cmake_minimum_required(VERSION 3.16)
project(sunset CXX)

# C++20 is needed on Windows for std::chrono timezone support
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    # official SDL3 VC devel package; point CMAKE_PREFIX_PATH at its directory
    find_package(SDL3 CONFIG REQUIRED)
else()
    # SDL3 on this system is installed in /usr/local; its exported CMake config
    # is broken, so locate it through pkg-config instead.
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
    add_library(SDL3::SDL3 ALIAS PkgConfig::SDL3)
endif()

set(IMGUI_DIR src/vendor/imgui)
add_executable(sunset
    src/main.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/imgui_impl_sdlrenderer3.cpp
)
target_include_directories(sunset PRIVATE ${IMGUI_DIR})
target_link_libraries(sunset PRIVATE SDL3::SDL3)


