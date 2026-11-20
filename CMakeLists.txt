cmake_minimum_required(VERSION 3.25)

project(nnhge VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Platform selection. devkitPro's toolchain file sets NINTENDO_3DS.
if(NINTENDO_3DS)
    set(NNH_PLATFORM "3ds")
else()
    set(NNH_PLATFORM "pc")
endif()
message(STATUS "NNHGE platform: ${NNH_PLATFORM}")

# Fast rebuilds: use a compiler cache when one is installed.
find_program(NNH_CCACHE NAMES ccache sccache)
if(NNH_CCACHE)
    message(STATUS "NNHGE compiler cache: ${NNH_CCACHE}")
    set(CMAKE_C_COMPILER_LAUNCHER "${NNH_CCACHE}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${NNH_CCACHE}")
endif()

option(NNH_BUILD_EXAMPLES "Build the examples" ON)
if(NNH_PLATFORM STREQUAL "pc")
    option(NNH_BUILD_TESTS "Build the unit tests" ON)
else()
    set(NNH_BUILD_TESTS OFF)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(nnh_game)

# The games manifest the nnh tool reads (spec 010): nnh_add_game() appends one line per game.
file(WRITE "${CMAKE_BINARY_DIR}/nnh_games.txt" "")

add_subdirectory(third_party/stb_vorbis)
add_subdirectory(engine)
add_subdirectory(link) # shared by the nnh tool and the NNH Link app (spec 011)

if(NNH_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(NNH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

add_subdirectory(apps) # NNH Link (spec 011); after tests: its test target uses the test framework

# The nnh command-line tool (spec 010): a host program, so only in the PC presets.
if(NNH_PLATFORM STREQUAL "pc")
    add_subdirectory(tools/nnh)
endif()
