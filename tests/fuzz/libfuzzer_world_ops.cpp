// libFuzzer entry for worlds (spec 020): operations against the reference model (built with -DNNH_BUILD_FUZZERS=ON
// and clang).
#include "fuzz_targets.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    return nnh::fuzz::world_ops_target(data, size);
}
