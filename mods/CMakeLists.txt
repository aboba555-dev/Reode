# Reode's own mods, shipped with releases. They use only the Reode SDK and
# functions exported by libcocos2d.dll (see common/mod_util.h).

function(reode_add_bundled_mod target id dir)
  reode_add_mod(${target}
    ID ${id}
    MOD_JSON ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/mod.json
    SOURCES ${dir}/src/main.cc common/mod_util.h)
  target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common)
  target_link_libraries(${target} PRIVATE reode_compile_options)
endfunction()

reode_add_bundled_mod(mod_fast_transitions reode.fast-transitions fast_transitions)
reode_add_bundled_mod(mod_screenshot reode.screenshot screenshot)
target_link_libraries(mod_screenshot PRIVATE opengl32 windowscodecs ole32 shell32)
reode_add_bundled_mod(mod_clock reode.clock clock)
