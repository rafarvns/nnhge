# cmake -DICON=<file.png> [-DSTAMP=<file>] -P nnh_check_icon.cmake
#
# Fails, naming the file, unless ICON is a 48x48 PNG: the size of the 3DS icon in the Homebrew Launcher
# and the HOME Menu (spec 009). Reads only the PNG header. Writes STAMP on success.

cmake_minimum_required(VERSION 3.25) # -P scripts don't inherit the project's policies
include(${CMAKE_CURRENT_LIST_DIR}/nnh_media_info.cmake)

if(NOT ICON)
    message(FATAL_ERROR "nnh_check_icon: ICON is required")
endif()
get_filename_component(_name "${ICON}" NAME)
if(NOT EXISTS "${ICON}")
    message(FATAL_ERROR "${_name}: file not found (${ICON})")
endif()
nnh_png_size("${ICON}" _w _h)
if(_w STREQUAL "")
    message(FATAL_ERROR "${_name}: not a PNG file (the icon must be a 48x48 PNG)")
endif()
if(NOT _w EQUAL 48 OR NOT _h EQUAL 48)
    message(FATAL_ERROR "${_name} is ${_w}x${_h}: the icon must be a 48x48 PNG")
endif()
if(STAMP)
    file(WRITE "${STAMP}" "48x48\n")
endif()
