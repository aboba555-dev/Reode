# Public SDK: headers only. Mods link against this, never against Reode.dll.
add_library(reode_sdk INTERFACE)
target_include_directories(reode_sdk INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(reode_sdk INTERFACE cxx_std_23)

# Everything except the DLL entry point, so unit tests can link it directly.
add_library(reode_core STATIC
  src/core/config.cc
  src/core/config.h
  src/core/console.cc
  src/core/console.h
  src/core/logger.cc
  src/core/logger.h
  src/core/paths.cc
  src/core/paths.h
  src/core/safety.cc
  src/core/safety.h
  src/game/cocos.cc
  src/game/cocos.h
  src/game/game_version.cc
  src/game/game_version.h
  src/game/menu_button.cc
  src/game/menu_button.h
  src/game/module_image.cc
  src/game/module_image.h
  src/game/pattern.cc
  src/game/pattern.h
  src/game/rtti.cc
  src/game/rtti.h
  src/geode_compat/geode_compat.cc
  src/geode_compat/geode_compat.h
  src/geode_compat/pe_imports.cc
  src/geode_compat/pe_imports.h
  src/hook/hook_manager.cc
  src/hook/hook_manager.h
  src/hook/near_allocator.cc
  src/hook/near_allocator.h
  src/hook/thread_freezer.cc
  src/hook/thread_freezer.h
  src/hook/x64_decoder.cc
  src/hook/x64_decoder.h
  src/mods/api_impl.cc
  src/mods/api_impl.h
  src/mods/mod_manager.cc
  src/mods/mod_manager.h
  src/mods/mod_metadata.cc
  src/mods/mod_metadata.h
  src/mods/mod_package.cc
  src/mods/mod_package.h
  src/mods/version.cc
  src/mods/version.h)
target_include_directories(reode_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(reode_core PUBLIC
  REODE_GEODE_COMPAT=$<BOOL:${REODE_GEODE_COMPAT}>)
target_link_libraries(reode_core
  PUBLIC reode_sdk reode_compile_options
  PRIVATE nlohmann_json::nlohmann_json reode_miniz dbghelp
    reode_geode_shim_host)

add_library(reode_loader SHARED src/entry.cc)
target_link_libraries(reode_loader PRIVATE reode_core)
target_compile_definitions(reode_loader PRIVATE
  REODE_VERSION_STRING="${PROJECT_VERSION}")
set_target_properties(reode_loader PROPERTIES OUTPUT_NAME "Reode")
