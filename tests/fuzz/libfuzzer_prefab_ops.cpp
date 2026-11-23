// libFuzzer entry for prefabs (spec 021): prefab spawns against hand-written ones (built with -DNNH_BUILD_FUZZERS=ON
// and clang).
#include "fuzz_targets.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    return nnh::fuzz::prefab_ops_target(data, size);
}
