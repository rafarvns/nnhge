// libFuzzer entry for `.nnhsprite` sprite libraries: validator, copy and playback (built with -DNNH_BUILD_FUZZERS=ON
// and clang).
#include "fuzz_targets.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    return nnh::fuzz::sprite_library_target(data, size);
}
