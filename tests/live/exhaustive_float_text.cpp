// Every one of the 2^32 float bit patterns (spec 030 T1.4, FR-13): each finite float's text reads back to the same bits
// through std::from_chars, and non-finite ones print inf/-inf/nan. Slow (minutes): ctest live_float_text_exhaustive,
// label "slow", run on the build VM (T9.1) or with -DNNH_SLOW_TESTS=ON.

#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "live/live_format.hpp"

int main() {
    using namespace nnh::detail::live;
    std::uint64_t failures = 0;
    std::uint32_t bits = 0;
    do {
        const float v = bits_float(bits);
        char out[kValueTextBytes];
        const int n = format_float(v, out);
        if ((bits & 0x7F800000u) == 0x7F800000u) {
            const char* want = (bits & 0x007FFFFFu) != 0 ? "nan" : ((bits >> 31) != 0 ? "-inf" : "inf");
            if (std::strcmp(out, want) != 0) ++failures;
        } else {
            float back = 0;
            const auto r = std::from_chars(out, out + n, back);
            if (r.ec != std::errc{} || r.ptr != out + n || float_bits(back) != bits) {
                if (failures < 10) std::printf("live float text: %08X -> \"%s\"\n", static_cast<unsigned>(bits), out);
                ++failures;
            }
        }
        ++bits;
    } while (bits != 0);
    std::printf("live float text exhaustive: %s (%llu failures)\n", failures == 0 ? "PASS" : "FAIL",
                static_cast<unsigned long long>(failures));
    return failures == 0 ? 0 : 1;
}
