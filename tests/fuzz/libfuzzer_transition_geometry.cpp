// libFuzzer entry for the transition effect geometry (spec 023; built with -DNNH_BUILD_FUZZERS=ON and clang).
#include "fuzz_transition_geometry.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    return nnh::fuzz::transition_geometry_target(data, size);
}
