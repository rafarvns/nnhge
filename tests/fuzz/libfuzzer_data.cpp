// libFuzzer entry for converted data assets: validator and apply (built with -DNNH_BUILD_FUZZERS=ON and
// clang).
#include "fuzz_targets.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    return nnh::fuzz::data_target(data, size);
}
