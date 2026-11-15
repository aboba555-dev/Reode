# Third-party dependencies. All are permissively licensed (MIT / BSD-3).

FetchContent_Declare(nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

# miniz ships as an amalgamated miniz.c / miniz.h pair in its release zip.
# SOURCE_SUBDIR points nowhere so only the sources are fetched; the target is
# defined below.
FetchContent_Declare(miniz
  URL https://github.com/richgel999/miniz/releases/download/3.0.2/miniz-3.0.2.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SOURCE_SUBDIR reode-no-cmake)

set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json miniz)

add_library(reode_miniz STATIC ${miniz_SOURCE_DIR}/miniz.c)
target_include_directories(reode_miniz SYSTEM PUBLIC ${miniz_SOURCE_DIR})
target_compile_definitions(reode_miniz PUBLIC
  MINIZ_NO_ZLIB_COMPATIBLE_NAMES _CRT_SECURE_NO_WARNINGS)

if(REODE_BUILD_TESTS)
  FetchContent_Declare(googletest
    URL https://github.com/google/googletest/releases/download/v1.15.2/googletest-1.15.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()
