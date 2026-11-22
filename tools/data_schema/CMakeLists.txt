# The data schema programs (spec 014 FR-3, decision D29; cmake/nnh_data.cmake): a game's schema sources + main()
# (schema_main.cpp) + core's pure registry files, compiled for the host. Built two ways:
#   - PC presets: add_subdirectory() from cmake/nnh_host_tools.cmake. Defines only the static library
#     nnh_data_schema_support (main and the pure files); each game's program links its schema objects to it, so
#     the schema sources are compiled once, for the game and the program.
#   - cross presets (3DS): a project of its own, one per game, built by ExternalProject with NNH_HOST_CXX:
#     -DNNH_SCHEMA_PROGRAM=<name> -DNNH_SCHEMA_SOURCES=<a|b> -DNNH_SCHEMA_INCLUDES=<dir|dir> ('|' separates).
# Explicit source lists; the pure files are also listed in nnh_data.cmake (_NNH_DATA_HOST_SOURCES), whose
# changes rebuild the program.

cmake_minimum_required(VERSION 3.25)
project(nnh_data_schema LANGUAGES CXX)

set(_nnh_root ${CMAKE_CURRENT_LIST_DIR}/../..)
if(NOT COMMAND nnh_engine_options)
    include(${_nnh_root}/cmake/nnh_engine_options.cmake)
endif()
set(_core ${_nnh_root}/engine/core/src)
set(_support_sources
    ${CMAKE_CURRENT_LIST_DIR}/schema_main.cpp
    ${_core}/core/crc32_update.cpp
    ${_core}/data/enum_names.cpp
    ${_core}/data/schema_check.cpp
    ${_core}/data/schema_fingerprint.cpp
    ${_core}/data/schema_registry.cpp
    ${_core}/data/schema_write.cpp
)

# MinGW: static C++ runtime, as nnh-assetc (Git for Windows puts an older libstdc++-6.dll first on the PATH).
function(_nnh_schema_program_options target)
    set_target_properties(${target} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    target_include_directories(${target} PRIVATE ${_core} ${_nnh_root}/engine/core/include)
    nnh_engine_options(${target})
    if(CMAKE_CXX_PLATFORM_ID STREQUAL "MinGW")
        target_link_options(${target} PRIVATE -static)
    endif()
endfunction()

if(NOT PROJECT_IS_TOP_LEVEL)
    add_library(nnh_data_schema_support STATIC ${_support_sources})
    set_target_properties(nnh_data_schema_support PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
                                                             CXX_EXTENSIONS OFF)
    target_include_directories(nnh_data_schema_support PUBLIC ${_core} ${_nnh_root}/engine/core/include)
    nnh_engine_options(nnh_data_schema_support)
    return()
endif()

foreach(var NNH_SCHEMA_PROGRAM NNH_SCHEMA_SOURCES)
    if(NOT ${var})
        message(FATAL_ERROR "tools/data_schema: ${var} is required (set by cmake/nnh_data.cmake)")
    endif()
endforeach()
string(REPLACE "|" ";" _sources "${NNH_SCHEMA_SOURCES}")
string(REPLACE "|" ";" _includes "${NNH_SCHEMA_INCLUDES}")
add_executable(${NNH_SCHEMA_PROGRAM} ${_sources} ${_support_sources})
target_include_directories(${NNH_SCHEMA_PROGRAM} PRIVATE ${_includes})
_nnh_schema_program_options(${NNH_SCHEMA_PROGRAM})
set_target_properties(${NNH_SCHEMA_PROGRAM} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
