// libFuzzer entry for NNH Link's frames target (built with -DNNH_BUILD_FUZZERS=ON and clang).
#include "link_targets.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    return nnh::link::fuzz::frames_target(data, size);
}
