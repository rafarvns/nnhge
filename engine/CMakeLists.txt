add_library(nnh_engine STATIC
    src/core/app.cpp
    src/core/debug.cpp
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

if(NNH_PLATFORM STREQUAL "3ds")
    target_sources(nnh_engine PRIVATE src/platform/3ds/platform_3ds.cpp)
    target_link_libraries(nnh_engine PUBLIC citro2d citro3d ctru m)
else()
    include(nnh_sdl3)
    target_sources(nnh_engine PRIVATE src/platform/pc/platform_pc.cpp)
    target_link_libraries(nnh_engine PRIVATE SDL3::SDL3)
    if(MINGW)
        # The simulator should run without the MinGW runtime DLLs on PATH.
        target_link_options(nnh_engine INTERFACE -static-libgcc -static-libstdc++)
    endif()
endif()
