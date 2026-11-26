# nnh::platform_pc — the PC simulator (SDL3). See README.md.

include(nnh_sdl3)

add_library(nnh_platform_pc STATIC
    src/app_pc.cpp
    src/assets_pc.cpp
    src/audio_mixer_pc.cpp
    src/audio_pc.cpp
    src/crash_pc.cpp
    src/debug_text_pc.cpp
    src/devtools_pc.cpp
    src/draw2d_pc.cpp
    src/draw_tiles_pc.cpp
    src/eyes_pc.cpp
    src/gamepad_pc.cpp
    src/input_override_pc.cpp
    src/input_pc.cpp
    src/language_pc.cpp
    src/log_pc.cpp
    src/memory_pc.cpp
    src/remote_address_pc.cpp
    src/remote_capture_pc.cpp
    src/remote_pc.cpp
    src/sim_layout.cpp
    src/sim_settings.cpp
    src/system_pc.cpp
    src/texels3d_pc.cpp
    src/texture_alpha_pc.cpp
    src/texture_pc.cpp
    src/time_pc.cpp
    src/touch_pc.cpp
    src/video_pc.cpp
    src/video3d_pc.cpp
    src/window_pc.cpp
    ../shared/stdio_fs.cpp
)
add_library(nnh::platform_pc ALIAS nnh_platform_pc)
# ../../core/src: the pure, header-only language tag rules (system/language_tag.hpp, spec 029); nothing of core is linked.
target_include_directories(nnh_platform_pc PRIVATE ../shared ../../core/src)
target_link_libraries(nnh_platform_pc PRIVATE nnh::core_contract SDL3::SDL3 nnh::raster3d) # raster3d: 3D (spec 026)
nnh_engine_options(nnh_platform_pc)

if(WIN32)
    # The console remote's loopback connection to nnh (remote_pc.cpp, spec 013): Winsock.
    target_link_libraries(nnh_platform_pc PRIVATE ws2_32)
endif()

if(MINGW)
    # Games should run without the MinGW runtime DLLs on PATH.
    target_link_options(nnh_platform_pc INTERFACE -static-libgcc -static-libstdc++)
endif()
