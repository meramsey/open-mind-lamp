cmake_minimum_required(VERSION 3.16)
project(oml-host-tests LANGUAGES C CXX)
enable_testing()
add_executable(core_test core_test.cpp ../main/lighting/core.cpp)
target_compile_features(core_test PRIVATE cxx_std_17)
target_include_directories(core_test PRIVATE ../main)
target_compile_options(core_test PRIVATE -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all)
target_link_options(core_test PRIVATE -fsanitize=undefined)
add_test(NAME core COMMAND core_test)

# Same cJSON 1.7.19 sources as the firmware dependency (the registry ~2 suffix
# packages ESP-IDF integration). Fetch once, pinned by immutable upstream commit.
include(FetchContent)
set(ENABLE_CJSON_TEST OFF CACHE BOOL "" FORCE)
set(ENABLE_CJSON_UTILS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(cjson
    GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
    GIT_TAG c859b25da02955fef659d658b8f324b5cde87be3)
FetchContent_MakeAvailable(cjson)
add_library(color_test_support STATIC
    ../main/lighting/core.cpp ../main/lighting/color.cpp ../main/lighting/calibration.cpp
    ../main/lighting/matrix.cpp ../main/api/color_json.cpp ../main/api/rng_json.cpp ../main/storage/settings.cpp
    platform.cpp)
target_compile_features(color_test_support PUBLIC cxx_std_17)
target_include_directories(color_test_support PUBLIC stubs ../main ${cjson_SOURCE_DIR})
target_link_libraries(color_test_support PUBLIC cjson)
target_compile_options(color_test_support PUBLIC -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all)
target_link_options(color_test_support PUBLIC -fsanitize=undefined)
add_executable(color_test color_test.cpp)
target_link_libraries(color_test PRIVATE color_test_support)
add_test(NAME color COMMAND color_test)
add_executable(color_fixture color_fixture.cpp)
target_link_libraries(color_fixture PRIVATE color_test_support)
add_executable(pipeline_test pipeline_test.cpp)
target_link_libraries(pipeline_test PRIVATE color_test_support)
add_test(NAME entropy_pipeline COMMAND pipeline_test)
add_executable(pipeline_fixture pipeline_fixture.cpp)
target_link_libraries(pipeline_fixture PRIVATE color_test_support)
