# nnh::platform_3ds — Nintendo 3DS (devkitPro: libctru, citro2d, citro3d). See README.md.

add_library(nnh_platform_3ds STATIC
    src/app_3ds.cpp
    src/assets_3ds.cpp
    src/audio_3ds.cpp
    src/console_3ds.cpp
    src/crash_3ds.cpp
    src/draw2d_3ds.cpp
    src/draw_tiles_3ds.cpp
    src/hbldr_args.cpp
    src/input_3ds.cpp
    src/lights3d_3ds.cpp
    src/launch_3ds.cpp
    src/log_3ds.cpp
    src/memory_3ds.cpp
    src/network_3ds.cpp
    src/pass3d_3ds.cpp
    src/remote_3ds.cpp
    src/remote_capture_3ds.cpp
    src/system_3ds.cpp
    src/texture_3ds.cpp
    src/tilt3d_3ds.cpp
    src/time_3ds.cpp
    src/video_3ds.cpp
    src/video3d_3ds.cpp
    ../shared/stdio_fs.cpp
    ../shared/raster3d/light_tables.cpp # the lighting tables the simulator uses too (spec 026)
)
add_library(nnh::platform_3ds ALIAS nnh_platform_3ds)
target_include_directories(nnh_platform_3ds PUBLIC include PRIVATE ../shared)
# Extension headers <nnh/3ds/...> check this to refuse non-3DS builds.
target_compile_definitions(nnh_platform_3ds PUBLIC NNH_PLATFORM_3DS)
target_include_directories(nnh_platform_3ds PRIVATE ../shared/raster3d)
set_source_files_properties(../shared/raster3d/light_tables.cpp PROPERTIES COMPILE_OPTIONS -ffp-contract=off)
# The 3D mesh shader (spec 026): PICA200 assembly built by devkitPro's picasso and embedded with bin2s; only
# video3d_3ds.cpp references it, so a 2D game links none of it.
ctr_add_shader_library(nnh_mesh3d_shader shaders/mesh3d.v.pica)
dkp_add_embedded_binary_library(nnh_mesh3d_shbin nnh_mesh3d_shader)
target_link_libraries(nnh_platform_3ds PRIVATE nnh::core_contract nnh_mesh3d_shbin citro2d citro3d ctru m)
nnh_engine_options(nnh_platform_3ds)
