# cmake -DIMAGE=<file.png> [-DSTAMP=<file>] -P nnh_check_image.cmake
#
# Fails unless IMAGE is a PNG whose width and height are powers of two from 8 to 1024 (the 3DS GPU's
# texture rule, enforced on every system: decision D4). Reads only the PNG header. Writes STAMP on
# success so the build can track it.

cmake_minimum_required(VERSION 3.25) # -P scripts don't inherit the project's policies

if(NOT IMAGE)
    message(FATAL_ERROR "nnh_check_image: IMAGE is required")
endif()
get_filename_component(_name "${IMAGE}" NAME)
if(NOT EXISTS "${IMAGE}")
    message(FATAL_ERROR "${_name}: file not found (${IMAGE})")
endif()

file(READ "${IMAGE}" _head LIMIT 24 HEX)
string(SUBSTRING "${_head}" 0 16 _signature)
string(SUBSTRING "${_head}" 24 8 _chunk)
if(NOT _signature STREQUAL "89504e470d0a1a0a" OR NOT _chunk STREQUAL "49484452")
    message(FATAL_ERROR "${_name}: not a PNG file (images in IMAGES must be PNG)")
endif()

# IHDR: width at bytes 16-19, height at 20-23, big endian.
string(SUBSTRING "${_head}" 32 8 _w)
string(SUBSTRING "${_head}" 40 8 _h)
math(EXPR _width "0x${_w}")
math(EXPR _height "0x${_h}")

set(_ok TRUE)
foreach(_side ${_width} ${_height})
    if(_side LESS 8 OR _side GREATER 1024)
        set(_ok FALSE)
    else()
        math(EXPR _bits "${_side} & (${_side} - 1)")
        if(NOT _bits EQUAL 0)
            set(_ok FALSE)
        endif()
    endif()
endforeach()
if(NOT _ok)
    message(FATAL_ERROR "${_name} is ${_width}x${_height}: textures need power-of-two sides from 8 to 1024 "
                        "(8, 16, 32, ..., 1024). Pad the image or pack it into a sprite sheet.")
endif()

if(STAMP)
    file(WRITE "${STAMP}" "${_width}x${_height}\n")
endif()
