# Provides the SDL3::SDL3 target for the PC simulator.
#
# Order: an SDL3 already installed on the system (find_package), then the official prebuilt
# development package (MinGW / MSVC, no compilation), then a static build from source (Linux/macOS).
# Downloads land in the build directory and are reused by later configures.

set(NNH_SDL3_VERSION "3.4.20")

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

set(_sdl_url "https://github.com/libsdl-org/SDL/releases/download/release-${NNH_SDL3_VERSION}")

if(MINGW)
    FetchContent_Declare(nnh_sdl3_prebuilt
        URL "${_sdl_url}/SDL3-devel-${NNH_SDL3_VERSION}-mingw.tar.gz"
        URL_HASH SHA256=d2d3bf3fb97a90b22e908a306d7362631ef1bab229d988fe6669595dc29cd9ed
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR _none_)
    FetchContent_MakeAvailable(nnh_sdl3_prebuilt)
    find_package(SDL3 CONFIG REQUIRED PATHS "${nnh_sdl3_prebuilt_SOURCE_DIR}" NO_DEFAULT_PATH)
elseif(MSVC)
    FetchContent_Declare(nnh_sdl3_prebuilt
        URL "${_sdl_url}/SDL3-devel-${NNH_SDL3_VERSION}-VC.zip"
        URL_HASH SHA256=1ec3009655ee37b1f73e52b3b120acc5afd8abaf9532f3303b1bf1dfe91b5969
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
        URL "${_sdl_url}/SDL3-${NNH_SDL3_VERSION}.tar.gz"
        URL_HASH SHA256=f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(nnh_sdl3)
endif()
