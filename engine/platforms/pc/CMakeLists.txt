# nnh::platform_pc — the PC simulator (SDL3). See README.md.

include(nnh_sdl3)

add_library(nnh_platform_pc STATIC
    src/app_pc.cpp
    src/crash_pc.cpp
    src/debug_text_pc.cpp
    src/devtools_pc.cpp
    src/gamepad_pc.cpp
    src/input_override_pc.cpp
    src/input_pc.cpp
    src/log_pc.cpp
    src/memory_pc.cpp
    src/system_pc.cpp
    src/time_pc.cpp
    src/touch_pc.cpp
    src/video_pc.cpp
    ../shared/stdio_fs.cpp
)
add_library(nnh::platform_pc ALIAS nnh_platform_pc)
target_include_directories(nnh_platform_pc PRIVATE ../shared)
target_link_libraries(nnh_platform_pc PRIVATE nnh::core_contract SDL3::SDL3)
nnh_engine_options(nnh_platform_pc)

if(MINGW)
    # Games should run without the MinGW runtime DLLs on PATH.
    target_link_options(nnh_platform_pc INTERFACE -static-libgcc -static-libstdc++)
endif()
