// libFuzzer entry for the reload receiver (spec 031 T1.6, AC-6): run on the build VM under ASan/UBSan.

#include <cstddef>
#include <cstdint>

#include "fuzz_targets.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    return nnh::fuzz::reload_receiver_target(data, size);
}
