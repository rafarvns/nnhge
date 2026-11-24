// libFuzzer entry for transforms (spec 022): random float bits into every transform function (built with -DNNH_BUILD_FUZZERS=ON and clang).
#include "fuzz_targets.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    return nnh::fuzz::transform_target(data, size);
}
