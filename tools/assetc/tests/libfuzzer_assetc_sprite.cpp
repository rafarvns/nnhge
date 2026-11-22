// libFuzzer entry for `nnh-assetc sprite`: a whole Aseprite conversion from memory (spec 016).
#include "fuzz_assetc.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    return nnh::assetc::fuzz::sprite_target(data, size);
}
