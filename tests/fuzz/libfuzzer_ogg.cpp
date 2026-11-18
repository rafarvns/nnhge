// libFuzzer entry for the OGG reader (built with -DNNH_BUILD_FUZZERS=ON and clang).
#include "fuzz_targets.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    return nnh::fuzz::ogg_target(data, size);
}
