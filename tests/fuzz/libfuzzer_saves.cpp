// libFuzzer entry for structured saves (spec 027; built with -DNNH_BUILD_FUZZERS=ON and clang).
#include "fuzz_saves.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    return nnh::fuzz::saves_target(data, size);
}
