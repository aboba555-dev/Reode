# A Reode mod. Build it with:
#
#   cmake -S . -B build -A x64 -DREODE_SDK_DIR=C:/path/to/reode-sdk
#   cmake --build build --config Release
#
# The result is build/Release/<id>.reode. Copy it into the reode/mods folder
# of your Geometry Dash installation.

cmake_minimum_required(VERSION 3.28)
project(MyReodeMod LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Mods use the release runtime, like the game and Reode itself.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

set(REODE_SDK_DIR "" CACHE PATH "Unpacked reode-sdk archive of a Reode release")
if(NOT EXISTS "${REODE_SDK_DIR}/cmake/ReodeMod.cmake")
  message(FATAL_ERROR "Set REODE_SDK_DIR to the unpacked reode-sdk archive")
endif()
include("${REODE_SDK_DIR}/cmake/ReodeMod.cmake")

reode_add_mod(my_mod
  ID "yourname.my-mod"
  MOD_JSON ${CMAKE_CURRENT_SOURCE_DIR}/mod.json
  SOURCES src/main.cc)
target_compile_options(my_mod PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(my_mod PRIVATE UNICODE _UNICODE NOMINMAX
  WIN32_LEAN_AND_MEAN)
