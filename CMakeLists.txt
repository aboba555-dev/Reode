cmake_minimum_required(VERSION 3.28)

project(Reode
  VERSION 0.1.0
  DESCRIPTION "Reode - mod loader for Geometry Dash"
  LANGUAGES C CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Reode targets 64-bit Windows only.")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

option(REODE_BUILD_TESTS "Build Reode unit tests" ON)
option(REODE_BUILD_EXAMPLES "Build example mods" ON)
# Geode.dll, the Geode compatibility layer, is compiled against headers
# generated from geode-sdk/bindings, which states no license (see NOTICE.md).
# Off by default, so release builds contain no Geode code.
option(REODE_GEODE_COMPAT "Build Geode.dll so Reode can run Geode mods" OFF)
set(REODE_GAME_DIR "" CACHE PATH
  "Geometry Dash install directory (used by the install target)")

include(FetchContent)
include(cmake/ReodeDependencies.cmake)
include(cmake/ReodeMod.cmake)

# Common warning / conformance flags for every Reode target.
add_library(reode_compile_options INTERFACE)
target_compile_options(reode_compile_options INTERFACE
  /W4 /permissive- /utf-8 /Zc:__cplusplus /Zc:preprocessor)
target_compile_definitions(reode_compile_options INTERFACE
  UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

add_subdirectory(proxy)
add_subdirectory(loader)
add_subdirectory(geode_shim)
add_subdirectory(installer)
add_subdirectory(mods)

if(REODE_BUILD_EXAMPLES)
  add_subdirectory(examples/hello_mod)
  add_subdirectory(examples/frame_trace)
endif()

if(REODE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(REODE_GAME_DIR)
  add_custom_target(install_to_game
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
      -File "${CMAKE_SOURCE_DIR}/tools/install.ps1"
      -GameDir "${REODE_GAME_DIR}"
      -BuildDir "$<TARGET_FILE_DIR:reode_loader>"
    DEPENDS reode_loader reode_proxy
      $<$<BOOL:${REODE_GEODE_COMPAT}>:reode_geode_shim>
    COMMENT "Installing Reode into ${REODE_GAME_DIR}")
endif()
