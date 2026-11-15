# XInput1_4.dll proxy. libcocos2d.dll imports XInput1_4.dll, so a copy in
# the game directory is loaded before the game starts. The proxy forwards
# every XInput call to the system DLL and bootstraps Reode.dll.

add_library(reode_proxy SHARED
  xinput_proxy.cc
  entry_hook.cc
  entry_hook.h
  xinput1_4.def)
target_link_libraries(reode_proxy PRIVATE reode_compile_options)
target_include_directories(reode_proxy PRIVATE ${PROJECT_SOURCE_DIR})
set_target_properties(reode_proxy PROPERTIES OUTPUT_NAME "XInput1_4")
