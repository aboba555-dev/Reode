# Reode's own implementation of the Geode runtime ABI, built as Geode.dll and
# installed to <game>/reode/compat/.
#
# Geode mods pass STL objects (std::string, std::shared_ptr,
# std::move_only_function, ...) across the DLL boundary, so the shim must use
# the release CRT and no iterator debugging in every configuration, exactly
# like the mods themselves.
#
# geode.def lists the Geode API symbols that are implemented. Regenerate it
# with tools/geode_shim/update_def.py after adding implementations.

# Only the host interface header, for Reode.dll. Always defined, so Reode.dll
# builds the same way with and without the compatibility layer.
add_library(reode_geode_shim_host INTERFACE)
target_include_directories(reode_geode_shim_host INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(NOT REODE_GEODE_COMPAT)
  return()
endif()

include(${PROJECT_SOURCE_DIR}/cmake/ReodeGeodeSdk.cmake)

add_library(reode_geode_shim SHARED
  geode.def
  ${REODE_GEODE_MATJSON_SOURCES}
  src/geode_abi_prelude.h
  src/geode_async.cc
  src/geode_axis_layout.cc
  src/geode_cocos_utils.cc
  src/geode_events.cc
  src/geode_hook.cc
  src/geode_impl.h
  src/geode_internals.h
  src/geode_json.cc
  src/geode_layout.cc
  src/geode_loader_api.cc
  src/geode_log.cc
  src/geode_metadata.cc
  src/geode_mod.cc
  src/geode_nine_slice.cc
  src/geode_node.cc
  src/geode_settings.cc
  src/geode_settings.h
  src/geode_simple_axis_layout.cc
  src/geode_utils.cc
  src/geode_version.cc
  src/geode_weakref.cc
  src/hook_dispatcher.cc
  src/hook_dispatcher.h
  src/host.cc
  src/host.h
  src/main_thread.cc
  src/main_thread.h
  src/mod_registry.cc
  src/mod_registry.h
  src/node_metadata.cc
  src/node_metadata.h
  src/shim_main.cc)
target_include_directories(reode_geode_shim
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set(reode_cocos_extra_lib ${CMAKE_CURRENT_BINARY_DIR}/cocos_extra.lib)
add_custom_command(
  OUTPUT ${reode_cocos_extra_lib}
  COMMAND ${CMAKE_AR} /nologo /machine:x64
    /def:${CMAKE_CURRENT_SOURCE_DIR}/cocos_extra.def
    /out:${reode_cocos_extra_lib}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/cocos_extra.def
  COMMENT "Generating cocos_extra.lib")
add_custom_target(reode_cocos_extra_lib DEPENDS ${reode_cocos_extra_lib})
add_dependencies(reode_geode_shim reode_cocos_extra_lib)

target_link_libraries(reode_geode_shim PRIVATE
  reode_geode_sdk reode_sdk
  ${REODE_GEODE_LINK_DIR}/libcocos2d.lib
  ${reode_cocos_extra_lib}
  bcrypt delayimp)
# Delay-loaded so that an export missing from a newer game's cocos2d only
# fails the call that uses it instead of the whole DLL.
target_link_options(reode_geode_shim PRIVATE /DELAYLOAD:libcocos2d.dll)
target_compile_definitions(reode_geode_shim PRIVATE UNICODE _UNICODE)
target_compile_options(reode_geode_shim PRIVATE
  "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/geode_abi_prelude.h")
# Third-party sources are compiled as they are, without their warnings.
set_source_files_properties(${REODE_GEODE_MATJSON_SOURCES} PROPERTIES
  COMPILE_OPTIONS /w)
# geode_version.cc declares VersionInfo dllexport itself, before any other
# Geode header, so it must not get geode_internals.h from the prelude.
set_source_files_properties(src/geode_version.cc PROPERTIES
  COMPILE_DEFINITIONS REODE_GEODE_NO_INTERNALS)
set_target_properties(reode_geode_shim PROPERTIES
  OUTPUT_NAME "Geode"
  MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
