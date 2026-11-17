# nnh::platform_3ds — Nintendo 3DS (devkitPro: libctru, citro2d, citro3d). See README.md.

add_library(nnh_platform_3ds STATIC
    src/app_3ds.cpp
    src/console_3ds.cpp
    src/crash_3ds.cpp
    src/debug_text_3ds.cpp
    src/input_3ds.cpp
    src/log_3ds.cpp
    src/memory_3ds.cpp
    src/system_3ds.cpp
    src/time_3ds.cpp
    src/video_3ds.cpp
    ../shared/stdio_fs.cpp
)
add_library(nnh::platform_3ds ALIAS nnh_platform_3ds)
target_include_directories(nnh_platform_3ds PUBLIC include PRIVATE ../shared)
# Extension headers <nnh/3ds/...> check this to refuse non-3DS builds.
target_compile_definitions(nnh_platform_3ds PUBLIC NNH_PLATFORM_3DS)
target_link_libraries(nnh_platform_3ds PRIVATE nnh::core_contract citro2d citro3d ctru m)
nnh_engine_options(nnh_platform_3ds)
