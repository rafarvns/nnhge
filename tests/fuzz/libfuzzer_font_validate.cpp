// libFuzzer entry for the .nnhfont validator (spec 029; built with -DNNH_BUILD_FUZZERS=ON and clang).
#include "fuzz_targets.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    return nnh::fuzz::font_validate_target(data, size);
}
