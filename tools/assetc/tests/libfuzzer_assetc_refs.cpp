// libFuzzer entry for the references sidecar reader and the data index step (fuzz_assetc.hpp).

#include "fuzz_assetc.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    return nnh::assetc::fuzz::refs_target(data, size);
}
