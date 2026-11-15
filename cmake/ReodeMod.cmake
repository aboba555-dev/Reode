# Reode mod SDK helpers.
#
#   reode_add_mod(<target>
#     ID <mod id>              # e.g. "author.my_mod"
#     MOD_JSON <path>          # mod.json shipped inside the package
#     SOURCES <files...>)
#
# Builds <id>.dll and packs it together with mod.json into <id>.reode
# (a plain zip archive) next to the binary.
#
# Works both inside the Reode source tree and from the SDK archive of a
# release (include/ next to cmake/), where it defines reode_sdk itself.

if(NOT TARGET reode_sdk AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/../include/reode/reode.h")
  add_library(reode_sdk INTERFACE)
  target_include_directories(reode_sdk INTERFACE "${CMAKE_CURRENT_LIST_DIR}/../include")
  target_compile_features(reode_sdk INTERFACE cxx_std_23)
endif()

function(reode_add_mod target)
  cmake_parse_arguments(ARG "" "ID;MOD_JSON" "SOURCES" ${ARGN})
  if(NOT ARG_ID OR NOT ARG_MOD_JSON)
    message(FATAL_ERROR "reode_add_mod: ID and MOD_JSON are required")
  endif()

  add_library(${target} SHARED ${ARG_SOURCES})
  target_link_libraries(${target} PRIVATE reode_sdk)
  set_target_properties(${target} PROPERTIES OUTPUT_NAME "${ARG_ID}")

  set(stage_dir "${CMAKE_CURRENT_BINARY_DIR}/${target}_package")
  set(package "$<TARGET_FILE_DIR:${target}>/${ARG_ID}.reode")
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${stage_dir}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${stage_dir}"
    COMMAND ${CMAKE_COMMAND} -E copy "${ARG_MOD_JSON}" "${stage_dir}/mod.json"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${target}>" "${stage_dir}/"
    COMMAND ${CMAKE_COMMAND} -E chdir "${stage_dir}"
      ${CMAKE_COMMAND} -E tar cf "${package}" --format=zip
      mod.json "$<TARGET_FILE_NAME:${target}>"
    COMMENT "Packing ${ARG_ID}.reode")
endfunction()
