# nnh_raster3d — the engine's CPU rasteriser (spec 026, decision D41): the PC simulator's 3D and the fake platform's
# pixel tests. Portable, no SDL. Optimised and without fused multiply-add in every flavour (-O2 after the flavour's
# flags wins), so Debug simulators stay usable and every host compiler gives the same pixels (FR-23).

add_library(nnh_raster3d STATIC
    clip.cpp
    light_tables.cpp
    raster.cpp
    raster3d.cpp
    sample.cpp
    shade.cpp
    vertex_stage.cpp
)
add_library(nnh::raster3d ALIAS nnh_raster3d)
target_include_directories(nnh_raster3d PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(nnh_raster3d PUBLIC nnh::core_contract)
nnh_engine_options(nnh_raster3d)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(nnh_raster3d PRIVATE -O2 -ffp-contract=off -fno-math-errno)
endif()
