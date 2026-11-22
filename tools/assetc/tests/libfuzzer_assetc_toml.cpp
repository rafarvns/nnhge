// libFuzzer entry for the TOML-subset reader of data files (built with -DNNH_BUILD_FUZZERS=ON and clang).
#include "fuzz_assetc.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    return nnh::assetc::fuzz::toml_target(data, size);
}
