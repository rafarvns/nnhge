// libFuzzer entry (spec 029 T4.10/T5.8): see fuzz_assetc.hpp, ttf_target.
#include "fuzz_assetc.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    return nnh::assetc::fuzz::ttf_target(data, size);
}
