# Provides the SDL3::SDL3 target for the PC simulator.
#
# Order: an SDL3 already installed on the system (find_package), then the official prebuilt
# development package (MinGW / MSVC, no compilation), then a static build from source (Linux/macOS).
# Downloads land in the build directory and are reused by later configures.

include("${CMAKE_CURRENT_LIST_DIR}/nnh_sdl3_pin.cmake") # NNH_SDL3_VERSION, the URLs and SHA-256s (also nnh doctor's)

find_package(SDL3 CONFIG QUIET)
if(SDL3_FOUND)
    message(STATUS "NNHGE: using system SDL3 ${SDL3_VERSION}")
    return()
endif()

include(FetchContent)

# Some Windows CMake builds (WinLibs, MSYS2) use GnuTLS without CA certificates, so HTTPS downloads
# fail with "certificate signer not trusted". Reuse Git for Windows' bundle instead of disabling TLS checks.
if(WIN32 AND NOT CMAKE_TLS_CAINFO AND NOT DEFINED ENV{CMAKE_TLS_CAINFO})
    file(TO_CMAKE_PATH "$ENV{ProgramFiles}" _pf)
    foreach(_ca "${_pf}/Git/mingw64/etc/ssl/certs/ca-bundle.crt"
                "${_pf}/Git/usr/ssl/certs/ca-bundle.crt")
        if(EXISTS "${_ca}")
            set(CMAKE_TLS_CAINFO "${_ca}")
            break()
        endif()
    endforeach()
endif()

if(MINGW)
    FetchContent_Declare(nnh_sdl3_prebuilt
        URL "${NNH_SDL3_MINGW_URL}"
        URL_HASH SHA256=${NNH_SDL3_MINGW_SHA256}
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR _none_)
    FetchContent_MakeAvailable(nnh_sdl3_prebuilt)
    find_package(SDL3 CONFIG REQUIRED PATHS "${nnh_sdl3_prebuilt_SOURCE_DIR}" NO_DEFAULT_PATH)
elseif(MSVC)
    FetchContent_Declare(nnh_sdl3_prebuilt
        URL "${NNH_SDL3_VC_URL}"
        URL_HASH SHA256=${NNH_SDL3_VC_SHA256}
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR _none_)
    FetchContent_MakeAvailable(nnh_sdl3_prebuilt)
    find_package(SDL3 CONFIG REQUIRED PATHS "${nnh_sdl3_prebuilt_SOURCE_DIR}" NO_DEFAULT_PATH)
else()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(nnh_sdl3
        URL "${NNH_SDL3_SOURCE_URL}"
        URL_HASH SHA256=${NNH_SDL3_SOURCE_SHA256}
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(nnh_sdl3)
endif()
