cmake_minimum_required(VERSION 3.16)

project(delta-studio LANGUAGES C CXX)

# Delta Studio (trimmed, cross-platform build used by Engine Simulator)
#
# The original engine targeted Win32 with Direct3D/DirectSound. This build
# uses SDL2 for windowing, input and audio and OpenGL for rendering so that a
# single code path works on Windows, Linux and macOS.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =========================================================
# SDL2

find_package(SDL2 CONFIG QUIET)
find_package(SDL2_image CONFIG QUIET)

if (NOT TARGET SDL2::SDL2 OR NOT (TARGET SDL2_image::SDL2_image OR TARGET SDL2_image::SDL2_image-static))
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2_PKG REQUIRED IMPORTED_TARGET sdl2)
    pkg_check_modules(SDL2_IMAGE_PKG REQUIRED IMPORTED_TARGET SDL2_image)
    set(DELTA_SDL2_LIBRARIES PkgConfig::SDL2_PKG PkgConfig::SDL2_IMAGE_PKG)
else()
    set(DELTA_SDL2_LIBRARIES SDL2::SDL2)
    if (TARGET SDL2_image::SDL2_image)
        list(APPEND DELTA_SDL2_LIBRARIES SDL2_image::SDL2_image)
    else()
        list(APPEND DELTA_SDL2_LIBRARIES SDL2_image::SDL2_image-static)
    endif()
endif()

# =========================================================
# OpenGL

set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)

# =========================================================
# Core

file(GLOB DELTA_CORE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB DELTA_CORE_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

add_library(delta-core STATIC ${DELTA_CORE_SOURCES} ${DELTA_CORE_HEADERS})

target_include_directories(delta-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/libraries/stb)

target_link_libraries(delta-core
    PUBLIC
        ${DELTA_SDL2_LIBRARIES}
        OpenGL::GL)

if (MSVC)
    target_compile_definitions(delta-core PUBLIC _CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
    # Delta relies on anonymous structs inside unions
    target_compile_options(delta-core PUBLIC
        $<$<COMPILE_LANGUAGE:CXX>:-fms-extensions -Wno-microsoft-anon-tag>)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(delta-core PUBLIC
            $<$<COMPILE_LANGUAGE:CXX>:-Wno-pedantic>)
    endif()
endif()

if (APPLE)
    # OpenGL is deprecated (but fully functional) on macOS
    target_compile_definitions(delta-core PUBLIC GL_SILENCE_DEPRECATION)
endif()

add_subdirectory(physics)
add_subdirectory(engines)
