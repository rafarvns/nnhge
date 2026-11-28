# ctest nnh_pin_check (spec 033 FR-9, T1.2): the release pin's configure-time check (nnh_release_pin.cmake) on good
# and bad values. Each case runs `cmake -P one.cmake` in a child, as nnh's configure would meet the entries.
#   cmake -P check.cmake

set(one "${CMAKE_CURRENT_LIST_DIR}/one.cmake")
set(hash "0123456789abcdef0123456789abcdef0123456789abcdef0123456789ABCDEF")
# url | sha256 | expected: "ok" or a regular expression of the error
set(cases
    "||ok"
    "https://github.com/x/nnhge/archive/v0.1.0.tar.gz|${hash}|ok"
    "file:///E:/x/nnhge.tar.gz|${hash}|ok"
    "https://example.org/a.tar.gz||NNH_RELEASE_ENGINE_URL and NNH_RELEASE_ENGINE_SHA256 go together"
    "|${hash}|NNH_RELEASE_ENGINE_URL and NNH_RELEASE_ENGINE_SHA256 go together"
    "http://example.org/a.tar.gz|${hash}|NNH_RELEASE_ENGINE_URL .* is not an https:// or file:// URL"
    "https://example.org/a b.tar.gz|${hash}|NNH_RELEASE_ENGINE_URL .* is not an https:// or file:// URL"
    "https://example.org/a<b>.tar.gz|${hash}|NNH_RELEASE_ENGINE_URL .* is not an https:// or file:// URL"
    "https://example.org/{a}.tar.gz|${hash}|NNH_RELEASE_ENGINE_URL .* is not an https:// or file:// URL"
    "https://example.org/a^b.tar.gz|${hash}|NNH_RELEASE_ENGINE_URL .* is not an https:// or file:// URL"
    "https://example.org/a.tar.gz|${hash}0|NNH_RELEASE_ENGINE_SHA256 .* is not 64 hex digits"
    "https://example.org/a.tar.gz|0123|NNH_RELEASE_ENGINE_SHA256 .* is not 64 hex digits"
    "https://example.org/a.tar.gz|zz23456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef|NNH_RELEASE_ENGINE_SHA256 .* is not 64 hex digits")
set(failures 0)
foreach(c IN LISTS cases)
    string(REGEX MATCH "^([^|]*)\\|([^|]*)\\|(.*)$" _ "${c}") # fields may be empty
    set(url "${CMAKE_MATCH_1}")
    set(sha "${CMAKE_MATCH_2}")
    set(want "${CMAKE_MATCH_3}")
    execute_process(COMMAND ${CMAKE_COMMAND} "-DURL=${url}" "-DSHA=${sha}" -P "${one}"
                    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
    string(REGEX REPLACE "[ \t\r\n]+" " " flat "${out}")
    if(want STREQUAL "ok")
        if(NOT rc EQUAL 0)
            message("pin_check: \"${url}\" / \"${sha}\" refused: ${flat}")
            math(EXPR failures "${failures} + 1")
        endif()
    elseif(rc EQUAL 0 OR NOT flat MATCHES "${want}")
        message("pin_check: \"${url}\" / \"${sha}\" should fail with \"${want}\" (exit ${rc}): ${flat}")
        math(EXPR failures "${failures} + 1")
    endif()
endforeach()
if(failures GREATER 0)
    message(FATAL_ERROR "pin_check: ${failures} case(s) failed")
endif()
list(LENGTH cases total)
message(STATUS "pin_check: ${total} cases as expected")
