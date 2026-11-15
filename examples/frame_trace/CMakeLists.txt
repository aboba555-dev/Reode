reode_add_mod(frame_trace
  ID "reode.frame_trace"
  MOD_JSON ${CMAKE_CURRENT_SOURCE_DIR}/mod.json
  SOURCES src/main.cc)
target_link_libraries(frame_trace PRIVATE reode_compile_options)
