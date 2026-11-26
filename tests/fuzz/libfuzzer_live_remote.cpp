// libFuzzer entry for the live values' remote target (spec 030 T5.3, AC-5): run on the build VM under ASan/UBSan.

#include <cstddef>
#include <cstdint>

namespace nnh::fuzz {
int live_remote_target(const std::uint8_t* data, std::size_t size);
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    return nnh::fuzz::live_remote_target(data, size);
}
