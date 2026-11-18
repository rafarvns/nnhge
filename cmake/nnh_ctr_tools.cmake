# nnh_ctr_tools() — makerom and bannertool for building .cia files (spec 009, decision D22).
#
# Called at configure time for games with nnh_add_game(... CIA ...). It downloads nothing: it sets
# NNH_MAKEROM_PROGRAM and NNH_BANNERTOOL_PROGRAM and, unless you give your own copies with the cache
# variables NNH_MAKEROM and NNH_BANNERTOOL (offline, macOS), adds the target `nnh_ctr_tools`, which fetches
# the pinned official releases into <build>/_tools/ the first time a .cia is built
# (nnh_fetch_ctr_tools.cmake). They are build tools: nothing of them ships inside a game.

set(_NNH_CTR_TOOLS_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(nnh_ctr_tools)
    if(NNH_MAKEROM AND NNH_BANNERTOOL)
        set(NNH_MAKEROM_PROGRAM "${NNH_MAKEROM}" PARENT_SCOPE)
        set(NNH_BANNERTOOL_PROGRAM "${NNH_BANNERTOOL}" PARENT_SCOPE)
        return()
    endif()
    # devkitPro's MSYS2 CMake reports "MSYS", not WIN32; the tools are native Windows programs either way.
    if(CMAKE_HOST_WIN32 OR CMAKE_HOST_SYSTEM_NAME MATCHES "^(MSYS|MINGW|CYGWIN|Windows)")
        set(host windows)
        set(ext ".exe")
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        set(host linux)
        set(ext "")
    else()
        message(FATAL_ERROR "NNHGE: no prebuilt makerom/bannertool for ${CMAKE_HOST_SYSTEM_NAME} "
                            "${CMAKE_HOST_SYSTEM_PROCESSOR}. Build them (docs/guides/packaging.md) and configure "
                            "with -DNNH_MAKEROM=<path> -DNNH_BANNERTOOL=<path>")
    endif()
    set(dir "${CMAKE_BINARY_DIR}/_tools")
    if(NOT TARGET nnh_ctr_tools)
        add_custom_target(nnh_ctr_tools
            COMMAND ${CMAKE_COMMAND} -DHOST=${host} -DDIR=${dir} -P ${_NNH_CTR_TOOLS_DIR}/nnh_fetch_ctr_tools.cmake
            COMMENT "makerom and bannertool (downloaded once, pinned)"
            VERBATIM)
    endif()
    set(NNH_MAKEROM_PROGRAM "${dir}/makerom${ext}" PARENT_SCOPE)
    set(NNH_BANNERTOOL_PROGRAM "${dir}/bannertool${ext}" PARENT_SCOPE)
endfunction()
