# The release pin of an nnh build (spec 033 FR-9, D56 item 3): NNH_RELEASE_ENGINE_URL and NNH_RELEASE_ENGINE_SHA256,
# set by spec 034's release build, written by `nnh new` as a new project's pinned engine. Both or neither; the URL
# https:// or file://; the SHA-256 64 hex digits. A wrong pin stops nnh's configure, naming the entry.
# Included by tools/nnh/CMakeLists.txt and by the ctest nnh_pin_check (tests/pin_check/check.cmake).

function(nnh_check_release_pin url sha256)
    if(url STREQUAL "" AND sha256 STREQUAL "")
        return()
    endif()
    if(url STREQUAL "" OR sha256 STREQUAL "")
        message(FATAL_ERROR "nnh: NNH_RELEASE_ENGINE_URL and NNH_RELEASE_ENGINE_SHA256 go together "
                            "(URL \"${url}\", SHA-256 \"${sha256}\"): set both or neither")
    endif()
    # The characters nnh doctor accepts in a URL it fetches (checks_network.cpp safe_url): letters, digits, -._~:/?#[]@!&'()*+,=%
    if(NOT url MATCHES "^(https|file)://[]A-Za-z0-9._~:/?#[@!&'()*+,=%-]+$")
        message(FATAL_ERROR "nnh: NNH_RELEASE_ENGINE_URL \"${url}\" is not an https:// or file:// URL")
    endif()
    if(NOT sha256 MATCHES "^[0-9a-fA-F]+$")
        message(FATAL_ERROR "nnh: NNH_RELEASE_ENGINE_SHA256 \"${sha256}\" is not 64 hex digits")
    endif()
    string(LENGTH "${sha256}" n)
    if(NOT n EQUAL 64)
        message(FATAL_ERROR "nnh: NNH_RELEASE_ENGINE_SHA256 \"${sha256}\" is not 64 hex digits")
    endif()
endfunction()
