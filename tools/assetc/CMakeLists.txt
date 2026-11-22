# nnh-assetc — the asset converter (spec 012, decision D27): a host program the build runs on asset files.
# Built two ways (cmake/nnh_host_tools.cmake):
#   - PC presets: add_subdirectory() from the top-level CMakeLists.txt, a normal target (plus its tests);
#   - cross presets (3DS): a project of its own, built for the host by ExternalProject with NNH_HOST_CXX.
# So this file must also work standalone. Explicit source lists.

cmake_minimum_required(VERSION 3.25)
project(nnh_assetc VERSION 0.1.0 LANGUAGES CXX)

set(_nnh_root ${CMAKE_CURRENT_LIST_DIR}/../..)
if(NOT COMMAND nnh_engine_options)
    include(${_nnh_root}/cmake/nnh_engine_options.cmake)
endif()
set(_core ${_nnh_root}/engine/core/src)

# Everything but main(), so the tests and the fuzz targets link the same code.
add_library(nnh_assetc_lib STATIC
    src/cli.cpp
    src/convert.cpp
    src/depfile.cpp
    src/files.cpp
    src/json.cpp
    src/json_fields.cpp
    src/json_parse.cpp
    src/json_string.cpp
    src/map_writer.cpp
    src/paths.cpp
    src/png_size.cpp
    src/tiled_layer_data.cpp
    src/tiled_map.cpp
    src/tiled_objects.cpp
    src/tiled_properties.cpp
    src/tiled_tileset.cpp
    # pure core files (no engine includes), shared with the console: the format, its layout and validator
    ${_core}/core/crc32.cpp
    ${_core}/tilemap/map_layout.cpp
    ${_core}/tilemap/map_property_table.cpp
    ${_core}/tilemap/map_strings.cpp
    ${_core}/tilemap/map_validate.cpp
    ${_core}/tilemap/map_validate_content.cpp
)
set_target_properties(nnh_assetc_lib PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_include_directories(nnh_assetc_lib PUBLIC src ${_core})
target_compile_definitions(nnh_assetc_lib PUBLIC NNH_ASSETC_VERSION="${PROJECT_VERSION}")
nnh_engine_options(nnh_assetc_lib)

add_executable(nnh_assetc src/main.cpp)
target_link_libraries(nnh_assetc PRIVATE nnh_assetc_lib)
set_target_properties(nnh_assetc PROPERTIES
    OUTPUT_NAME nnh-assetc
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
nnh_engine_options(nnh_assetc)
# MinGW: static C++ runtime, as tools/nnh (Git for Windows puts an older libstdc++-6.dll first on the PATH).
# Checked on the compiler, not the CMake host: in devkitPro's MSYS2 the host CMake is MSYS's, the compiler MinGW.
function(_nnh_assetc_static target)
    if(CMAKE_CXX_PLATFORM_ID STREQUAL "MinGW")
        target_link_options(${target} PRIVATE -static)
    endif()
endfunction()
_nnh_assetc_static(nnh_assetc)

if(PROJECT_IS_TOP_LEVEL)
    # Standalone (ExternalProject): the program lands at the top of the build folder, where
    # nnh_host_tools.cmake expects it.
    set_target_properties(nnh_assetc PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    return()
endif()

# Fuzz target bodies (tests/fuzz_assetc.cpp): the mini-fuzz in the tests and the libFuzzer programs share them.
if(NNH_BUILD_TESTS OR NNH_BUILD_FUZZERS)
    add_library(nnh_assetc_fuzz STATIC tests/fuzz_assetc.cpp)
    target_include_directories(nnh_assetc_fuzz PUBLIC tests)
    target_link_libraries(nnh_assetc_fuzz PUBLIC nnh_assetc_lib)
    nnh_engine_options(nnh_assetc_fuzz)
endif()

if(NNH_BUILD_TESTS)
    add_executable(nnh_assetc_tests
        tests/test_cli.cpp
        tests/test_depfile.cpp
        tests/test_fuzz_assetc.cpp
        tests/test_json.cpp
        tests/test_json_fields.cpp
        tests/test_map_writer.cpp
        tests/test_paths.cpp
        tests/test_tiled_map.cpp
        tests/test_tiled_objects.cpp
        tests/test_tiled_animations.cpp
        tests/test_tiled_tileset.cpp
        tests/tiled_fixture.cpp
    )
    target_link_libraries(nnh_assetc_tests PRIVATE nnh_assetc_fuzz nnh_assetc_lib nnh_test_framework)
    target_compile_definitions(nnh_assetc_tests PRIVATE NNH_ASSETC_TEST_TMP="${CMAKE_CURRENT_BINARY_DIR}")
    # test_cli.cpp writes a small game's assets here (the tool itself never creates folders: the build does).
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cli/assets/levels ${CMAKE_CURRENT_BINARY_DIR}/cli/assets/tiles
                        ${CMAKE_CURRENT_BINARY_DIR}/cli/out)
    nnh_engine_options(nnh_assetc_tests)
    _nnh_assetc_static(nnh_assetc_tests)
    add_test(NAME assetc_unit COMMAND nnh_assetc_tests)

    add_test(NAME assetc_version COMMAND nnh_assetc --version)
    set_tests_properties(assetc_version PROPERTIES PASS_REGULAR_EXPRESSION "^nnh-assetc 0[.]1[.]0 [(]crc32 CBF43926[)]")
    add_test(NAME assetc_usage COMMAND nnh_assetc)
    set_tests_properties(assetc_usage PROPERTIES WILL_FAIL TRUE)
endif()

# libFuzzer programs (clang only, like tests/): fuzz_assetc_json (the JSON reader) and fuzz_assetc_map (a whole
# conversion of an in-memory map + tileset).
if(NNH_BUILD_FUZZERS)
    foreach(lib nnh_assetc_lib nnh_assetc_fuzz)
        target_compile_options(${lib} PRIVATE -fsanitize=fuzzer-no-link,address)
    endforeach()
    foreach(name json map)
        add_executable(fuzz_assetc_${name} tests/libfuzzer_assetc_${name}.cpp)
        target_link_libraries(fuzz_assetc_${name} PRIVATE nnh_assetc_fuzz)
        target_compile_options(fuzz_assetc_${name} PRIVATE -fsanitize=fuzzer,address)
        target_link_options(fuzz_assetc_${name} PRIVATE -fsanitize=fuzzer,address)
    endforeach()
endif()
