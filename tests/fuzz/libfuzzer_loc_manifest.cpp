// libFuzzer entry for the .nnh_loc validator and its readers (spec 029; built with -DNNH_BUILD_FUZZERS=ON and clang).
#include "fuzz_targets.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    return nnh::fuzz::loc_manifest_target(data, size);
}
