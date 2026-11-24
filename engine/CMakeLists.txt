# NNHGE engine = portable core + one platform library per system (docs/architecture/platforms.md).
#
#   nnh::core_contract   platform contract headers (for platform libraries)
#   nnh::core            portable engine
#   nnh::platform_<sys>  implements the contract for one system (pc, 3ds; fake for tests)
#   nnh::engine          what games link: core + the platform selected by the preset

include(nnh_engine_options)

add_subdirectory(core)
if(NNH_PLATFORM STREQUAL "pc" OR NNH_BUILD_TESTS)
    add_subdirectory(platforms/shared/raster3d) # the CPU rasteriser (spec 026, D41): PC simulator and fake pixels
endif()
add_subdirectory(platforms/${NNH_PLATFORM})
if(NNH_BUILD_TESTS)
    add_subdirectory(platforms/fake)
endif()

add_library(nnh_engine INTERFACE)
add_library(nnh::engine ALIAS nnh_engine)
target_link_libraries(nnh_engine INTERFACE nnh::core nnh::platform_${NNH_PLATFORM})
