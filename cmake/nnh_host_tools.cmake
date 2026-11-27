# Host tools: programs the build runs on the developer's machine (spec 012, decision D27): nnh-assetc, and in PC
# presets the support library of the data schema programs (tools/data_schema, spec 014; cmake/nnh_data.cmake).
#
#   nnh_assetc_command(<out_command> <out_depends> [<what the game has>])
#       Sets <out_command> to the nnh-assetc program to put in a custom command's COMMAND, and <out_depends>
#       to what that custom command must DEPEND on (empty for a prebuilt program). Fails at configure time,
#       with what to do, when the program can't be had, naming what the game has ("Tiled maps" by default). For the
#       asset steps of games with Tiled maps, Aseprite exports (spec 016) or data assets (spec 014).
#
# The target nnh_assetc exists in every preset and is never part of a normal build of a cross preset:
#   - PC presets: the executable of tools/assetc (added by the top-level CMakeLists.txt, with its tests).
#   - cross presets (3DS): an ExternalProject (EXCLUDE_FROM_ALL) that configures tools/assetc as a project of
#     its own with the host compiler NNH_HOST_CXX, into build/<preset>/_host/assetc/. It only runs when a
#     custom command needs the program or someone builds the target; the first time it also detects the host
#     compiler (a few seconds); afterwards it runs only when one of the files it compiles changed.
#   - cross presets with -DNNH_ASSETC=<path>: use a prebuilt nnh-assetc (from a PC build:
#     build/pc-*/tools/assetc/), like NNH_MAKEROM. Nothing is built; the target just prints its version.
#
# NNH_HOST_CXX (cache, cross presets): the host C++ compiler. Default: the environment variable NNH_HOST_CXX
# (scripts/dkp.sh sets it on Windows, where devkitPro's MSYS2 has no host compiler), else c++/g++/clang++ on
# the PATH. If none is found, the 3DS build still configures and builds; only nnh_assetc (and games that
# need it) fail, saying how to fix it.

include_guard(GLOBAL)
include(ExternalProject)
include("${CMAKE_CURRENT_LIST_DIR}/nnh_platform.cmake")

set(NNH_ASSETC "" CACHE FILEPATH "Prebuilt nnh-assetc to use instead of building it (cross presets)")

# _nnh_host_cxx(<out_var>): the host C++ compiler (cross presets), "" if there is none. Shared by nnh-assetc and
# the data schema programs (nnh_data.cmake). Sets the cache variable NNH_HOST_CXX the first time it finds one.
function(_nnh_host_cxx out_var)
    if(NOT NNH_HOST_CXX)
        if(DEFINED ENV{NNH_HOST_CXX} AND NOT "$ENV{NNH_HOST_CXX}" STREQUAL "")
            set(_cxx "$ENV{NNH_HOST_CXX}")
        else()
            # Host programs, not the cross toolchain's: ignore its find root.
            find_program(_cxx NAMES c++ g++ clang++ NO_CACHE NO_CMAKE_FIND_ROOT_PATH)
        endif()
        if(_cxx)
            set(NNH_HOST_CXX "${_cxx}" CACHE FILEPATH "Host C++ compiler for host tools (cross presets)" FORCE)
        endif()
    endif()
    set(${out_var} "${NNH_HOST_CXX}" PARENT_SCOPE)
endfunction()

# Declares the target nnh_assetc. Called once by the top-level CMakeLists.txt.
function(nnh_host_tools_setup)
    if(NNH_PLATFORM STREQUAL "pc")
        add_subdirectory(${PROJECT_SOURCE_DIR}/tools/assetc ${PROJECT_BINARY_DIR}/tools/assetc)
        add_subdirectory(${PROJECT_SOURCE_DIR}/tools/data_schema ${PROJECT_BINARY_DIR}/tools/data_schema)
        return()
    endif()
    if(NNH_ASSETC)
        if(NOT EXISTS "${NNH_ASSETC}")
            message(FATAL_ERROR "NNH_ASSETC: ${NNH_ASSETC} not found")
        endif()
        add_custom_target(nnh_assetc COMMAND "${NNH_ASSETC}" --version VERBATIM)
        message(STATUS "NNHGE nnh-assetc: ${NNH_ASSETC} (prebuilt)")
        return()
    endif()

    _nnh_host_cxx(_cxx)
    if(NOT _cxx)
        set(_why "no host C++ compiler: set NNH_HOST_CXX (environment or -D), or -DNNH_ASSETC=<prebuilt nnh-assetc>")
        add_custom_target(nnh_assetc
            COMMAND ${CMAKE_COMMAND} -E echo "nnh-assetc: ${_why} (docs/tech/cmake.md)"
            COMMAND ${CMAKE_COMMAND} -E false VERBATIM)
        set_property(GLOBAL PROPERTY NNH_ASSETC_MISSING "${_why}")
        message(STATUS "NNHGE nnh-assetc: ${_why}")
        return()
    endif()

    set(_dir ${PROJECT_BINARY_DIR}/_host/assetc)
    if(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows|MSYS|CYGWIN|MINGW")
        set(_exe ${_dir}/nnh-assetc.exe)
    else()
        set(_exe ${_dir}/nnh-assetc)
    endif()
    ExternalProject_Add(nnh_assetc
        SOURCE_DIR ${PROJECT_SOURCE_DIR}/tools/assetc
        BINARY_DIR ${_dir}
        PREFIX ${PROJECT_BINARY_DIR}/_host/assetc-ep
        CMAKE_ARGS -DCMAKE_CXX_COMPILER=${_cxx} -DCMAKE_BUILD_TYPE=Release
                   -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
        BUILD_BYPRODUCTS ${_exe}
        INSTALL_COMMAND ""
        EXCLUDE_FROM_ALL TRUE
        USES_TERMINAL_CONFIGURE TRUE USES_TERMINAL_BUILD TRUE)
    # The sub-build runs only when a file it compiles changed (spec 031 T10.3): with BUILD_ALWAYS its three steps cost
    # ≈ 1.2 s of every 3DS asset build through MSYS, hot reload's included. Its sources are tools/assetc, the engine core
    # files and headers it compiles in and the vendored stb libraries; a new source file comes with a change of
    # tools/assetc/CMakeLists.txt, which is in the list.
    file(GLOB_RECURSE _assetc_inputs
         ${PROJECT_SOURCE_DIR}/tools/assetc/*.cpp ${PROJECT_SOURCE_DIR}/tools/assetc/*.hpp
         ${PROJECT_SOURCE_DIR}/tools/assetc/*.h ${PROJECT_SOURCE_DIR}/tools/assetc/CMakeLists.txt
         ${PROJECT_SOURCE_DIR}/engine/core/src/*.cpp ${PROJECT_SOURCE_DIR}/engine/core/src/*.hpp
         ${PROJECT_SOURCE_DIR}/engine/core/include/*.hpp
         ${PROJECT_SOURCE_DIR}/third_party/stb_truetype/* ${PROJECT_SOURCE_DIR}/third_party/stb_image/*)
    list(FILTER _assetc_inputs EXCLUDE REGEX "/tests/")
    ExternalProject_Add_StepDependencies(nnh_assetc build ${_assetc_inputs})
    set_property(GLOBAL PROPERTY NNH_ASSETC_PROGRAM "${_exe}")
    message(STATUS "NNHGE nnh-assetc: built on demand with ${_cxx}")
endfunction()

function(nnh_assetc_command out_command out_depends)
    set(_what "Tiled maps")
    if(ARGC GREATER 2)
        set(_what "${ARGV2}")
    endif()
    nnh_platform(_platform "nnh_assetc_command") # a game's folder has no NNH_PLATFORM variable (spec 032 FR-6)
    if(_platform STREQUAL "pc")
        set(${out_command} "$<TARGET_FILE:nnh_assetc>" PARENT_SCOPE)
        set(${out_depends} nnh_assetc PARENT_SCOPE)
        return()
    endif()
    if(NNH_ASSETC)
        set(${out_command} "${NNH_ASSETC}" PARENT_SCOPE)
        set(${out_depends} "" PARENT_SCOPE)
        return()
    endif()
    get_property(_why GLOBAL PROPERTY NNH_ASSETC_MISSING)
    set(_what "Tiled maps")
    if(ARGC GREATER 2)
        set(_what "${ARGV2}")
    endif()
    if(_why)
        message(FATAL_ERROR "This game has ${_what}, which need nnh-assetc: ${_why} (docs/tech/cmake.md)")
    endif()
    get_property(_exe GLOBAL PROPERTY NNH_ASSETC_PROGRAM)
    # The file (rebuilt -> maps reconverted) and the target (builds it first).
    set(${out_command} "${_exe}" PARENT_SCOPE)
    set(${out_depends} "${_exe};nnh_assetc" PARENT_SCOPE)
endfunction()
