# nnh-assetc — the asset converter (spec 012, decision D27): a host program the build runs on asset files.
# Built two ways (cmake/nnh_host_tools.cmake):
#   - PC presets: add_subdirectory() from the top-level CMakeLists.txt, a normal target;
#   - cross presets (3DS): a project of its own, built for the host by ExternalProject with NNH_HOST_CXX.
# So this file must also work standalone. Explicit source lists.

cmake_minimum_required(VERSION 3.25)
project(nnh_assetc VERSION 0.1.0 LANGUAGES CXX)

set(_nnh_root ${CMAKE_CURRENT_LIST_DIR}/../..)
if(NOT COMMAND nnh_engine_options)
    include(${_nnh_root}/cmake/nnh_engine_options.cmake)
endif()

add_executable(nnh_assetc
    src/main.cpp
    ${_nnh_root}/engine/core/src/core/crc32.cpp # pure core file (no engine includes), shared with the console
)
set_target_properties(nnh_assetc PROPERTIES
    OUTPUT_NAME nnh-assetc
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_include_directories(nnh_assetc PRIVATE src ${_nnh_root}/engine/core/src)
target_compile_definitions(nnh_assetc PRIVATE NNH_ASSETC_VERSION="${PROJECT_VERSION}")
nnh_engine_options(nnh_assetc)
# MinGW: static C++ runtime, as tools/nnh (Git for Windows puts an older libstdc++-6.dll first on the PATH).
# Checked on the compiler, not the CMake host: in devkitPro's MSYS2 the host CMake is MSYS's, the compiler MinGW.
if(CMAKE_CXX_PLATFORM_ID STREQUAL "MinGW")
    target_link_options(nnh_assetc PRIVATE -static)
endif()

if(PROJECT_IS_TOP_LEVEL)
    # Standalone (ExternalProject): the program lands at the top of the build folder, where
    # nnh_host_tools.cmake expects it.
    set_target_properties(nnh_assetc PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
elseif(NNH_BUILD_TESTS)
    add_test(NAME assetc_version COMMAND nnh_assetc --version)
    set_tests_properties(assetc_version PROPERTIES PASS_REGULAR_EXPRESSION "^nnh-assetc 0[.]1[.]0 [(]crc32 CBF43926[)]")
    add_test(NAME assetc_usage COMMAND nnh_assetc)
    set_tests_properties(assetc_usage PROPERTIES WILL_FAIL TRUE)
endif()
