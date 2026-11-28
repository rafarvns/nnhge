# The SDL3 the PC simulator pins (cmake/nnh_sdl3.cmake): its version and the package URLs and SHA-256s. Kept apart so
# `nnh doctor --online` lists exactly these URLs (tools/nnh/src/doctor/checks_network.cpp; a unit test compares them).

set(NNH_SDL3_VERSION "3.4.20")
set(NNH_SDL3_URL_BASE "https://github.com/libsdl-org/SDL/releases/download/release-${NNH_SDL3_VERSION}")
set(NNH_SDL3_MINGW_URL "${NNH_SDL3_URL_BASE}/SDL3-devel-${NNH_SDL3_VERSION}-mingw.tar.gz")
set(NNH_SDL3_MINGW_SHA256 d2d3bf3fb97a90b22e908a306d7362631ef1bab229d988fe6669595dc29cd9ed)
set(NNH_SDL3_VC_URL "${NNH_SDL3_URL_BASE}/SDL3-devel-${NNH_SDL3_VERSION}-VC.zip")
set(NNH_SDL3_VC_SHA256 1ec3009655ee37b1f73e52b3b120acc5afd8abaf9532f3303b1bf1dfe91b5969)
set(NNH_SDL3_SOURCE_URL "${NNH_SDL3_URL_BASE}/SDL3-${NNH_SDL3_VERSION}.tar.gz")
set(NNH_SDL3_SOURCE_SHA256 f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f)
