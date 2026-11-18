# ctest helper: resolves a game's package options like nnh_add_game() does and prints the result.
#   cmake -DTARGET=<name> [-DTITLE=...] [-DVERSION=...] [-DICON=...] [-DCIA=<CIA options, comma-separated>]
#         -P resolve_package.cmake
cmake_minimum_required(VERSION 3.25)
include(${CMAKE_CURRENT_LIST_DIR}/../../../cmake/nnh_package_rules.cmake)
if(NOT DEFINED TITLE)
    set(TITLE "${TARGET}")
endif()
set(_cia_args "")
if(DEFINED CIA)
    string(REPLACE "," ";" CIA "${CIA}") # ctest splits arguments at ";"
    set(_cia_args CIA ${CIA})
endif()
set(_optional "")
foreach(_key VERSION ICON)
    if(${_key})
        list(APPEND _optional ${_key} "${${_key}}")
    endif()
endforeach()
nnh_resolve_package(R TARGET "${TARGET}" SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}" TITLE "${TITLE}"
                    DESCRIPTION "Made with NNHGE" AUTHOR "Someone" ${_optional} ${_cia_args})
get_filename_component(_icon "${R_ICON}" NAME)
get_filename_component(_banner "${R_BANNER}" NAME)
get_filename_component(_audio "${R_BANNER_AUDIO}" NAME)
message("version=${R_VERSION_MAJOR}.${R_VERSION_MINOR}.${R_VERSION_MICRO} icon=${_icon} cia=${R_CIA} "
        "id=${R_UNIQUE_ID} code=${R_PRODUCT_CODE} banner=${_banner} audio=${_audio}")
