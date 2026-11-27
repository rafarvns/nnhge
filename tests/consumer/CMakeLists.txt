# The consumer game project (spec 032 FR-16): a game project laid out like docs/guides/game-projects.md, kept with the
# engine's tests. It brings the engine in like a game would and builds games from nested folders, every
# nnh_add_game() keyword and every asset kind included. tests/consumer.cmake builds it from ctest
# (tests/consumer_tools/run_consumer.cmake); tests/consumer_tools/copy_project.sh copies it out of the repository.
#
# CONSUMER_WAY (a test switch, not something a game needs):
#   fetch  (default) FetchContent of the engine: the URL and SHA-256 in engine_pin.cmake (written by the tests, not
#          committed), or the local engine in FETCHCONTENT_SOURCE_DIR_NNHGE (the presets map NNHGE_ENGINE_DIR to it).
#   subdir add_subdirectory(${CONSUMER_ENGINE_SUBDIR}): a git submodule or a copied folder.
cmake_minimum_required(VERSION 3.25)
project(nnh_consumer LANGUAGES C CXX)

set(CONSUMER_WAY "fetch" CACHE STRING "How the engine comes in: fetch or subdir (tests only)")
if(CONSUMER_WAY STREQUAL "subdir")
    add_subdirectory("${CONSUMER_ENGINE_SUBDIR}" _nnhge)
else()
    include(FetchContent)
    include("${CMAKE_CURRENT_SOURCE_DIR}/engine_pin.cmake" OPTIONAL) # CONSUMER_ENGINE_URL, CONSUMER_ENGINE_SHA256
    if(NOT CONSUMER_ENGINE_URL AND NOT FETCHCONTENT_SOURCE_DIR_NNHGE)
        message(FATAL_ERROR "nnh_consumer: set NNHGE_ENGINE_DIR to an engine checkout, or write engine_pin.cmake "
                            "(tests/consumer_tools/copy_project.sh)")
    endif()
    if(NOT CONSUMER_ENGINE_URL)
        set(CONSUMER_ENGINE_URL "file://unused/nnhge.tar.gz") # never downloaded: the local engine overrides it
        set(CONSUMER_ENGINE_SHA256 "0000000000000000000000000000000000000000000000000000000000000000")
    endif()
    FetchContent_Declare(nnhge
        URL "${CONSUMER_ENGINE_URL}"
        URL_HASH SHA256=${CONSUMER_ENGINE_SHA256}
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(nnhge)
endif()

# FR-10: the engine's version, readable after it is added.
if(DEFINED CONSUMER_EXPECT_VERSION)
    set(_parts "${NNHGE_VERSION_MAJOR}.${NNHGE_VERSION_MINOR}.${NNHGE_VERSION_PATCH}")
    if(NOT NNHGE_VERSION STREQUAL CONSUMER_EXPECT_VERSION OR NOT _parts STREQUAL CONSUMER_EXPECT_VERSION)
        message(FATAL_ERROR "nnh_consumer: NNHGE_VERSION is \"${NNHGE_VERSION}\" (${_parts}), "
                            "expected ${CONSUMER_EXPECT_VERSION}")
    endif()
    message(STATUS "nnh_consumer: NNHGE_VERSION ${NNHGE_VERSION} as expected")
endif()
if(NNHGE_VERSION VERSION_LESS 0.1)
    message(FATAL_ERROR "nnh_consumer needs NNHGE 0.1 or later (found \"${NNHGE_VERSION}\")")
endif()

# The project's own tests: the engine's must not appear here (FR-4).
enable_testing()
add_test(NAME consumer_own_test COMMAND ${CMAKE_COMMAND} -E echo "the consumer project's own test")

add_subdirectory(plain) # a library of the project's own, not a game (FR-8)
add_subdirectory(game)  # the full game, from a nested folder (FR-6)
add_subdirectory(small) # a game of hello's size (AC-9's budgets)
