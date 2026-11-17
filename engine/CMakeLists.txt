# The engine library. Sources are listed explicitly (no file(GLOB)); see docs/tech/cmake.md.

add_library(nnh_engine STATIC
    src/app/app.cpp
    src/debug/debug.cpp
    src/gfx/gfx.cpp
    src/input/input.cpp
)
add_library(nnh::engine ALIAS nnh_engine)

target_include_directories(nnh_engine
    PUBLIC include
    PRIVATE src)

target_compile_features(nnh_engine PUBLIC cxx_std_20)

# Engine code builds without exceptions and RTTI (smaller, predictable code on ARM11).
if(MSVC)
    target_compile_options(nnh_engine PRIVATE /W4 /GR- /EHs-c-)
else()
    target_compile_options(nnh_engine PRIVATE -Wall -Wextra -fno-exceptions -fno-rtti)
endif()

# Platform backend: one file per module, selected at build time (link-time Bridge, no virtual).
if(NNH_PLATFORM STREQUAL "3ds")
    target_sources(nnh_engine PRIVATE
        src/platform/3ds/debug_text_3ds.cpp
        src/platform/3ds/input_3ds.cpp
        src/platform/3ds/video_3ds.cpp)
    target_link_libraries(nnh_engine PUBLIC citro2d citro3d ctru m)
else()
    include(nnh_sdl3)
    target_sources(nnh_engine PRIVATE
        src/platform/pc/debug_text_pc.cpp
        src/platform/pc/input_pc.cpp
        src/platform/pc/video_pc.cpp)
    target_link_libraries(nnh_engine PRIVATE SDL3::SDL3)
    if(MINGW)
        # The simulator should run without the MinGW runtime DLLs on PATH.
        target_link_options(nnh_engine INTERFACE -static-libgcc -static-libstdc++)
    endif()
endif()
