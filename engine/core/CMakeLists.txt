# nnh::core — the portable engine. Never depends on a system SDK.
# Sources are listed explicitly (no file(GLOB)); see docs/tech/cmake.md.

# Platform contract: what every platform library implements and nnh::core calls.
add_library(nnh_core_contract INTERFACE)
add_library(nnh::core_contract ALIAS nnh_core_contract)
target_include_directories(nnh_core_contract INTERFACE contract include)

add_library(nnh_core STATIC
    src/app/app.cpp
    src/debug/debug.cpp
    src/gfx/gfx.cpp
    src/input/input.cpp
    src/time/clock.cpp
    src/time/fixed_step.cpp
)
add_library(nnh::core ALIAS nnh_core)
target_include_directories(nnh_core PUBLIC include PRIVATE src)
target_link_libraries(nnh_core PRIVATE nnh::core_contract)
nnh_engine_options(nnh_core)

# Build flavor (docs/architecture/foundation.md#build-flavors), PUBLIC so games and platform
# libraries see the same value through <nnh/build.hpp>:
#   Debug -> Debug, RelWithDebInfo -> Development, Release/MinSizeRel -> Release.
set(_nnh_flavor_definitions
    $<$<CONFIG:Debug>:NNH_FLAVOR_DEBUG>
    $<$<CONFIG:RelWithDebInfo>:NNH_FLAVOR_DEVELOPMENT>
    $<$<CONFIG:Release,MinSizeRel>:NNH_FLAVOR_RELEASE>)
target_compile_definitions(nnh_core PUBLIC ${_nnh_flavor_definitions})
target_compile_definitions(nnh_core_contract INTERFACE ${_nnh_flavor_definitions})
