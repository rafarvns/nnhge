// libFuzzer entry for pathfinding (spec 025 T6.3; built with -DNNH_BUILD_FUZZERS=ON and clang).
#include "fuzz_path_ops.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    return nnh::fuzz::path_ops_target(data, size);
}
