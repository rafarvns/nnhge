// libFuzzer entry for `nnh-assetc data`: a whole conversion, and the schema description reader (built with
// -DNNH_BUILD_FUZZERS=ON and clang).
#include "fuzz_assetc.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    return nnh::assetc::fuzz::data_target(data, size);
}
