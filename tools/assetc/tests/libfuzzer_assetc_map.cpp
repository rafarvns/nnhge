// libFuzzer entry for a whole nnh-assetc map conversion: map '\0' tileset (built with -DNNH_BUILD_FUZZERS=ON and
// clang).
#include "fuzz_assetc.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    return nnh::assetc::fuzz::map_target(data, size);
}
