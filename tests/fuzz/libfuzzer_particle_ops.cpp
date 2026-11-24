// libFuzzer entry for particles (spec 024 T5.5; built with -DNNH_BUILD_FUZZERS=ON and clang).
#include "fuzz_particle_ops.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    return nnh::fuzz::particle_ops_target(data, size);
}
