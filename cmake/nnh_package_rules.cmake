# Package metadata rules (spec 009), include()d by nnh_game.cmake and the ctest driver
# tests/data/package/check_package.cmake.
#
#   nnh_check_package_values(UNIQUE_ID <0x...> PRODUCT_CODE <CTR-X-XXXX> VERSION <a.b.c>
#                            TITLE <text> DESCRIPTION <text> AUTHOR <text>)
#
# Fails with a message naming the field for anything a .cia (or the SMDH) can't hold: a unique ID outside
# the homebrew range, a malformed product code or version, text longer than the SMDH allows.

# Characters in a UTF-8 string: bytes that aren't continuation bytes (0x80-0xBF).
function(nnh_utf8_length text out)
    string(HEX "${text}" hex)
    string(REGEX MATCHALL ".." bytes "${hex}")
    set(n 0)
    foreach(b IN LISTS bytes)
        if(NOT b MATCHES "^[89ab]")
            math(EXPR n "${n} + 1")
        endif()
    endforeach()
    set(${out} ${n} PARENT_SCOPE)
endfunction()

function(nnh_check_package_values)
    cmake_parse_arguments(PARSE_ARGV 0 P "" "UNIQUE_ID;PRODUCT_CODE;VERSION;TITLE;DESCRIPTION;AUTHOR" "")
    if(NOT P_UNIQUE_ID MATCHES "^0[xX][0-9A-Fa-f]+$")
        message(FATAL_ERROR "UNIQUE_ID ${P_UNIQUE_ID} must be hexadecimal, like 0xE0A01")
    endif()
    math(EXPR id "${P_UNIQUE_ID}")
    if(id LESS 768 OR id GREATER 1015807) # 0x00300 .. 0xF7FFF
        message(FATAL_ERROR "UNIQUE_ID ${P_UNIQUE_ID} is outside the homebrew range 0x00300-0xF7FFF")
    endif()
    if(NOT P_PRODUCT_CODE MATCHES "^CTR-[A-Z]-[A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9]$")
        message(FATAL_ERROR "PRODUCT_CODE ${P_PRODUCT_CODE} must look like CTR-H-ABCD (upper case)")
    endif()
    set(version_ok FALSE)
    if(P_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
        if(CMAKE_MATCH_1 LESS_EQUAL 63 AND CMAKE_MATCH_2 LESS_EQUAL 63 AND CMAKE_MATCH_3 LESS_EQUAL 15)
            set(version_ok TRUE)
        endif()
    endif()
    if(NOT version_ok)
        message(FATAL_ERROR "VERSION ${P_VERSION} must be major.minor.micro, at most 63.63.15")
    endif()
    foreach(field_limit TITLE:64 DESCRIPTION:128 AUTHOR:64)
        string(REPLACE ":" ";" pair "${field_limit}")
        list(GET pair 0 field)
        list(GET pair 1 limit)
        nnh_utf8_length("${P_${field}}" n)
        if(n GREATER limit)
            message(FATAL_ERROR "${field} is ${n} characters: at most ${limit}")
        endif()
    endforeach()
endfunction()
