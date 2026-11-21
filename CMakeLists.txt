cmake_minimum_required(VERSION 3.20)

project(fried_editor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# The version the editor reports, which a release sets to the one it is published as.
set(FRIED_EDITOR_VERSION "${PROJECT_VERSION}" CACHE STRING "The version the editor reports")

include(cmake/CoreIsUiFree.cmake)
fried_editor_check_core_is_ui_free(${CMAKE_CURRENT_SOURCE_DIR}/core ${CMAKE_CURRENT_SOURCE_DIR}/tests)

# The logic shared with the Project Manager, which also brings nlohmann/json and stb.
add_subdirectory(third_party/fried_core)
add_subdirectory(core)

enable_testing()
add_subdirectory(tests)

# The editor is optional, so the core and its tests build where SDL2 or OpenGL are not installed.
option(FRIED_EDITOR_UI "Build the editor application" ON)
if(FRIED_EDITOR_UI)
    # SDL2's own CMake package where there is one, as on Windows, and pkg-config elsewhere.
    find_package(SDL2 CONFIG QUIET)
    if(TARGET SDL2::SDL2)
        set(FRIED_SDL2_TARGET SDL2::SDL2)
    else()
        find_package(PkgConfig)
        if(PkgConfig_FOUND)
            pkg_check_modules(FRIED_SDL2 IMPORTED_TARGET sdl2)
        endif()
        if(FRIED_SDL2_FOUND)
            set(FRIED_SDL2_TARGET PkgConfig::FRIED_SDL2)
        endif()
    endif()
    find_package(OpenGL)
    if(FRIED_SDL2_TARGET AND OpenGL_FOUND)
        add_subdirectory(ui)
        add_subdirectory(app)
    else()
        message(STATUS "SDL2 or OpenGL not found, so the editor application is not built")
    endif()
endif()
