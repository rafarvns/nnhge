// libFuzzer entry for hierarchies (spec 022): prefab hierarchies against hand-written ones (built with -DNNH_BUILD_FUZZERS=ON and clang).
#include "fuzz_targets.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    return nnh::fuzz::hierarchy_ops_target(data, size);
}
