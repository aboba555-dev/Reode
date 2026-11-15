# Official Geode SDK headers, used by geode_shim only for binary
# compatibility: Geode mods inline template code from these headers, so the
# objects our Geode.dll creates must have exactly the same layout.
#
# Licenses of what is fetched here:
#   Geode SDK, TulipHook, result, matjson: BSL-1.0
#   bindings: no license stated (only used to generate headers at build
#   time; see NOTICE.md)
#   fmt, asp2, arc: MIT       nontype_functional: BSD-2-Clause
#
# The SDK version must match the Geode ABI the target mods were built for.

set(REODE_GEODE_SDK_VERSION "5.11.2")
# GD version whose bindings are used to generate headers. The shim never
# calls game functions, so this only has to match the SDK headers.
set(REODE_GEODE_BINDINGS_GD "2.2081")

# A macro, not a function, so that <name>_SOURCE_DIR reaches the caller.
macro(reode_fetch_source name url)
  FetchContent_Declare(${name}
    URL ${url}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR reode-no-cmake)
  FetchContent_MakeAvailable(${name})
endmacro()

reode_fetch_source(geode_sdk
  https://github.com/geode-sdk/geode/archive/refs/tags/v${REODE_GEODE_SDK_VERSION}.tar.gz)
reode_fetch_source(geode_bindings
  https://github.com/geode-sdk/bindings/archive/1605f500d053f8de122bf5c7dc6d17c24866e02e.tar.gz)
reode_fetch_source(geode_tulip
  https://github.com/geode-sdk/TulipHook/archive/refs/tags/v3.1.15.tar.gz)
reode_fetch_source(geode_result
  https://github.com/geode-sdk/result/archive/refs/tags/v1.4.1.tar.gz)
reode_fetch_source(geode_matjson
  https://github.com/geode-sdk/json/archive/refs/tags/v3.3.1.tar.gz)
reode_fetch_source(geode_nontype
  https://github.com/geode-sdk/nontype_functional/archive/6d1d08cb447c038e3e56812dd362e61abc92933a.tar.gz)
reode_fetch_source(geode_fmt
  https://github.com/fmtlib/fmt/archive/refs/tags/12.1.0.tar.gz)
reode_fetch_source(geode_asp
  https://github.com/dankmeme01/asp2/archive/ecea33bad2d9804054888bbd12db9573b33aaebe.tar.gz)
reode_fetch_source(geode_arc
  https://github.com/dankmeme01/arc/archive/refs/tags/v1.5.11.tar.gz)

set(REODE_GEODE_INCLUDE ${geode_sdk_SOURCE_DIR}/loader/include)
set(REODE_GEODE_CODEGEN_OUT ${CMAKE_BINARY_DIR}/geode_codegen)

# Build Geode's bindings code generator (a host tool) and generate the game
# class headers that the SDK headers include.
if(NOT EXISTS ${REODE_GEODE_CODEGEN_OUT}/Geode/GeneratedBinding.hpp)
  set(codegen_build ${CMAKE_BINARY_DIR}/geode_codegen_build)
  string(REPLACE "\\" "/" codegen_include "${REODE_GEODE_INCLUDE}")
  message(STATUS "Building Geode bindings codegen")
  execute_process(
    COMMAND ${CMAKE_COMMAND}
      -S ${geode_bindings_SOURCE_DIR}/codegen -B ${codegen_build}
      -G ${CMAKE_GENERATOR} -A x64
      "-DCMAKE_CXX_FLAGS=/EHsc /I${codegen_include}"
    COMMAND_ERROR_IS_FATAL ANY)
  execute_process(
    COMMAND ${CMAKE_COMMAND} --build ${codegen_build} --config Release
    COMMAND_ERROR_IS_FATAL ANY)
  message(STATUS "Generating Geode bindings for GD ${REODE_GEODE_BINDINGS_GD}")
  execute_process(
    COMMAND ${codegen_build}/Release/Codegen.exe Win64
      ${geode_bindings_SOURCE_DIR}/bindings/${REODE_GEODE_BINDINGS_GD}
      ${REODE_GEODE_CODEGEN_OUT}
      --sdk-version=${REODE_GEODE_SDK_VERSION}
    COMMAND_ERROR_IS_FATAL ANY)
endif()

# Compiled dependencies of the SDK headers. They are built with the release
# CRT like the shim itself (see geode_shim/CMakeLists.txt).
file(GLOB reode_geode_fmt_sources
  ${geode_fmt_SOURCE_DIR}/src/format.cc ${geode_fmt_SOURCE_DIR}/src/os.cc)
# matjson is compiled into Geode.dll itself: Geode mods import it from there.
file(GLOB REODE_GEODE_MATJSON_SOURCES ${geode_matjson_SOURCE_DIR}/src/*.cpp)
file(GLOB reode_geode_asp_sources ${geode_asp_SOURCE_DIR}/src/time/*.cpp)
set(REODE_GEODE_LINK_DIR ${REODE_GEODE_INCLUDE}/link/win64)

add_library(reode_geode_sdk STATIC
  ${reode_geode_fmt_sources}
  ${reode_geode_asp_sources})
target_include_directories(reode_geode_sdk SYSTEM PUBLIC
  ${REODE_GEODE_INCLUDE}
  ${REODE_GEODE_INCLUDE}/Geode/cocos/include
  ${REODE_GEODE_INCLUDE}/Geode/cocos/extensions
  ${REODE_GEODE_INCLUDE}/Geode/fmod
  ${REODE_GEODE_CODEGEN_OUT}
  ${geode_bindings_SOURCE_DIR}/bindings/include
  ${geode_tulip_SOURCE_DIR}/include
  ${geode_result_SOURCE_DIR}/include
  ${geode_matjson_SOURCE_DIR}/include
  ${geode_nontype_SOURCE_DIR}/include
  ${geode_fmt_SOURCE_DIR}/include
  ${geode_asp_SOURCE_DIR}/include
  ${geode_arc_SOURCE_DIR}/include)
target_compile_definitions(reode_geode_sdk PUBLIC
  GEODE_COMP_GD_VERSION=22081
  MAT_JSON_DYNAMIC=1
  MAT_JSON_EXPORTING
  NOMINMAX
  WIN32_LEAN_AND_MEAN
  _ITERATOR_DEBUG_LEVEL=0)
target_compile_options(reode_geode_sdk PUBLIC
  /EHsc /bigobj /utf-8 /Zc:__cplusplus /Zc:preprocessor /permissive-)
target_compile_features(reode_geode_sdk PUBLIC cxx_std_23)
set_property(TARGET reode_geode_sdk PROPERTY
  MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
