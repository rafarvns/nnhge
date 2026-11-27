cmake_minimum_required(VERSION 3.25)

project(nnhge VERSION 0.1.0 LANGUAGES C CXX)

# The engine works as a CMake subproject of a game project (FetchContent, add_subdirectory: spec 032, D55;
# docs/guides/game-projects.md). Everything below that a game reads is a cache entry, a global property or a function;
# nothing is written into this source folder.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(nnh_version) # NNHGE_VERSION* (spec 032 FR-10)

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
set_property(GLOBAL PROPERTY NNH_PLATFORM "${NNH_PLATFORM}") # for nnh_add_game() in a game's folders (nnh_platform.cmake)

# Fast rebuilds: use a compiler cache when one is installed. -DNNH_CCACHE= (empty) turns it off; nnh_add_game() gives it to
# a game project's game targets too (nnh_game_defaults.cmake).
if(NOT DEFINED NNH_CCACHE OR NNH_CCACHE MATCHES "-NOTFOUND$")
    find_program(NNH_CCACHE NAMES ccache sccache)
endif()
if(NNH_CCACHE)
    message(STATUS "NNHGE compiler cache: ${NNH_CCACHE}")
    set(CMAKE_C_COMPILER_LAUNCHER "${NNH_CCACHE}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${NNH_CCACHE}")
endif()

# The engine's optional parts (spec 032 FR-4): ON in the engine repository, OFF when the engine is a game project's
# subproject (PROJECT_IS_TOP_LEVEL); a game may still turn any on. The host programs (nnh-assetc, the data schema
# support) have no option: a game's build steps need them.
option(NNH_BUILD_EXAMPLES "Build the examples" ${PROJECT_IS_TOP_LEVEL})
option(NNH_BUILD_APPS "Build the apps (NNH Link)" ${PROJECT_IS_TOP_LEVEL})
if(NNH_PLATFORM STREQUAL "pc")
    option(NNH_BUILD_TESTS "Build the unit, conformance and fuzz tests" ${PROJECT_IS_TOP_LEVEL})
    option(NNH_BUILD_TOOLS "Build the nnh tool (and its tests with NNH_BUILD_TESTS)" ${PROJECT_IS_TOP_LEVEL})
else()
    set(NNH_BUILD_TESTS OFF)
    set(NNH_BUILD_TOOLS OFF) # the nnh tool is a host program
endif()

include(nnh_game)
include(nnh_optional_parts)

# The games manifest the nnh tool reads (spec 010): nnh_add_game() appends one line per game.
file(WRITE "${CMAKE_BINARY_DIR}/nnh_games.txt" "")

add_subdirectory(third_party/stb_vorbis)
add_subdirectory(third_party/ryu)
add_subdirectory(engine)
if(NNH_BUILD_APPS OR NNH_BUILD_TOOLS OR NNH_BUILD_TESTS)
    add_subdirectory(link) # shared by the nnh tool and the NNH Link app (spec 011); no game needs it
endif()

if(NNH_BUILD_TESTS)
    enable_testing() # before any folder that adds tests (tools/assetc is added by the host tools below)
endif()

# nnh-assetc (spec 012, D27): built in PC presets, built for the host on demand in cross presets. Before the
# examples: a game's asset step with Tiled maps needs the program (nnh_assetc_command()).
include(nnh_host_tools)
nnh_host_tools_setup()

nnh_optional_part(NNH_BUILD_EXAMPLES examples)
nnh_optional_part(NNH_BUILD_TESTS tests)
nnh_optional_part(NNH_BUILD_APPS apps) # NNH Link (spec 011); after tests: its test target uses the test framework
nnh_optional_part(NNH_BUILD_TOOLS tools/nnh) # the nnh command-line tool (spec 010): PC presets only
