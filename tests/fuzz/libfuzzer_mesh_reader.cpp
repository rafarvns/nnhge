// libFuzzer entry for the .nnhmesh reader (spec 026 FR-7; built with -DNNH_BUILD_FUZZERS=ON and clang).
#include "fuzz_targets.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    return nnh::fuzz::mesh_reader_target(data, size);
}
