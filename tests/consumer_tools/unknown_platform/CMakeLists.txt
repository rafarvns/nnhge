# A scratch project whose engine platform is empty or unsupported (spec 032 FR-6, AC-12): it includes the engine's
# nnh_game.cmake, sets the global property NNH_PLATFORM (the test-only override) to PLATFORM_VALUE, or leaves it
# unset, and calls nnh_add_game(). The configure must fail with the FR-6 message before any game target exists.
cmake_minimum_required(VERSION 3.25)
project(unknown_platform LANGUAGES NONE)
if(NOT PLATFORM_VALUE STREQUAL "<unset>")
    set_property(GLOBAL PROPERTY NNH_PLATFORM "${PLATFORM_VALUE}")
endif()
include("${ENGINE}/cmake/nnh_game.cmake")
nnh_add_game(probe_game SOURCES main.cpp)
