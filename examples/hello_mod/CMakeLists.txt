reode_add_mod(hello_mod
  ID "reode.hello"
  MOD_JSON ${CMAKE_CURRENT_SOURCE_DIR}/mod.json
  SOURCES src/main.cc)
target_link_libraries(hello_mod PRIVATE reode_compile_options)
