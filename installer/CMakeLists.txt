# ReodeSetup.exe: a one-click installer that carries Reode.dll, the
# XInput1_4.dll proxy, the bundled mods and, with REODE_GEODE_COMPAT, Geode.dll
# as embedded resources.
#
# Release builds are what players get; a Debug installer embeds debug DLLs,
# which need the Visual Studio debug runtime.

add_library(reode_setup_core STATIC src/setup.cc src/setup.h)
target_include_directories(reode_setup_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(reode_setup_core PUBLIC reode_compile_options)

# The resource script names the files of the current configuration.
set(reode_setup_rc ${CMAKE_CURRENT_BINARY_DIR}/setup_$<CONFIG>.rc)
file(GENERATE OUTPUT ${reode_setup_rc} CONTENT
"#include \"resource_ids.h\"
REODE_RESOURCE_REODE_DLL RCDATA \"$<TARGET_FILE:reode_loader>\"
REODE_RESOURCE_PROXY_DLL RCDATA \"$<TARGET_FILE:reode_proxy>\"
$<$<BOOL:${REODE_GEODE_COMPAT}>:REODE_RESOURCE_GEODE_DLL RCDATA \"$<TARGET_FILE:reode_geode_shim>\">
REODE_RESOURCE_MOD_FAST_TRANSITIONS RCDATA \"$<TARGET_FILE_DIR:mod_fast_transitions>/reode.fast-transitions.reode\"
REODE_RESOURCE_MOD_SCREENSHOT RCDATA \"$<TARGET_FILE_DIR:mod_screenshot>/reode.screenshot.reode\"
REODE_RESOURCE_MOD_CLOCK RCDATA \"$<TARGET_FILE_DIR:mod_clock>/reode.clock.reode\"
")

add_executable(reode_setup WIN32 src/main.cc src/resource_ids.h ${reode_setup_rc})
target_include_directories(reode_setup PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(reode_setup PRIVATE reode_setup_core comctl32 ole32)
add_dependencies(reode_setup reode_loader reode_proxy
  mod_fast_transitions mod_screenshot mod_clock)
set(reode_setup_embedded
  "$<TARGET_FILE:reode_loader>;$<TARGET_FILE:reode_proxy>;$<TARGET_FILE:mod_fast_transitions>;$<TARGET_FILE:mod_screenshot>;$<TARGET_FILE:mod_clock>")
if(REODE_GEODE_COMPAT)
  add_dependencies(reode_setup reode_geode_shim)
  string(APPEND reode_setup_embedded ";$<TARGET_FILE:reode_geode_shim>")
endif()
# Re-embed the files whenever they are rebuilt.
set_source_files_properties(${reode_setup_rc} PROPERTIES OBJECT_DEPENDS
  "${reode_setup_embedded}")
set_target_properties(reode_setup PROPERTIES OUTPUT_NAME "ReodeSetup")
