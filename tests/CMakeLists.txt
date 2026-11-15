add_executable(reode_tests
  hook_manager_test.cc
  mod_metadata_test.cc
  pattern_test.cc
  rtti_test.cc
  safety_test.cc
  setup_test.cc
  version_test.cc
  x64_decoder_test.cc)
target_link_libraries(reode_tests PRIVATE reode_core reode_setup_core
  GTest::gtest_main)
# Hook tests patch functions of this executable; incremental linking would
# route them through jump thunks, which is supported but less representative.
target_link_options(reode_tests PRIVATE /INCREMENTAL:NO)

include(GoogleTest)
gtest_discover_tests(reode_tests DISCOVERY_MODE PRE_TEST)
