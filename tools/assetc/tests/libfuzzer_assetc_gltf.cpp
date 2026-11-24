// libFuzzer entry for `nnh-assetc mesh`: a whole glTF conversion from memory (spec 026).
#include "fuzz_assetc.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    return nnh::assetc::fuzz::gltf_target(data, size);
}
