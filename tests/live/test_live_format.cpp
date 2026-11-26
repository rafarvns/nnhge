// Value text (spec 030 T1.4, FR-13, FR-22): hand-written expected texts, and every printed float read back to the same
// bits through std::from_chars (seeded floats, every power of two, the subnormal edges).

#include <charconv>
#include <cstdint>
#include <cstring>
#include <limits>

#include "live/live_format.hpp"
#include "test.hpp"

using namespace nnh::detail::live;

namespace {

bool float_is(float v, const char* expected) {
    char out[kValueTextBytes];
    const int n = format_float(v, out);
    const bool ok = n == static_cast<int>(std::strlen(expected)) && std::strcmp(out, expected) == 0;
    if (!ok) std::printf("    format_float: got \"%s\", expected \"%s\"\n", out, expected);
    return ok;
}

bool round_trips(float v) {
    char out[kValueTextBytes];
    const int n = format_float(v, out);
    float back = 0;
    const auto r = std::from_chars(out, out + n, back);
    const bool ok = r.ec == std::errc{} && r.ptr == out + n && float_bits(back) == float_bits(v);
    if (!ok) std::printf("    round trip: %08X -> \"%s\"\n", static_cast<unsigned>(float_bits(v)), out);
    return ok;
}

} // namespace

NNH_TEST(live_format_plain_decimals) {
    CHECK(float_is(5.25f, "5.25"));
    CHECK(float_is(0.1f, "0.1"));
    CHECK(float_is(1.0f, "1"));
    CHECK(float_is(-2.5f, "-2.5"));
    CHECK(float_is(0.0f, "0"));
    CHECK(float_is(-0.0f, "-0"));
    CHECK(float_is(16777216.0f, "16777216"));
    CHECK(float_is(123456789.0f, "123456790"));
    CHECK(float_is(120000000.0f, "120000000"));
    CHECK(float_is(0.0001f, "0.0001"));
}

NNH_TEST(live_format_exponent_boundary_on_both_sides) {
    CHECK(float_is(0.00001f, "0.00001"));
    CHECK(float_is(0.000015f, "0.000015"));
    CHECK(float_is(0.000001f, "1e-06"));
    CHECK(float_is(0.0000015f, "1.5e-06"));
    CHECK(float_is(100000000.0f, "100000000"));
    CHECK(float_is(999999940.0f, "999999940"));
    CHECK(float_is(1000000000.0f, "1e+09"));
    CHECK(float_is(1500000000.0f, "1.5e+09"));
}

NNH_TEST(live_format_extremes) {
    CHECK(float_is(std::numeric_limits<float>::max(), "3.4028235e+38"));
    CHECK(float_is(-std::numeric_limits<float>::max(), "-3.4028235e+38"));
    CHECK(float_is(std::numeric_limits<float>::denorm_min(), "1e-45"));
    CHECK(float_is(std::numeric_limits<float>::min(), "1.1754944e-38"));
    CHECK(float_is(0.00000015f, "1.5e-07"));
}

NNH_TEST(live_format_non_finite_for_watches) {
    CHECK(float_is(std::numeric_limits<float>::infinity(), "inf"));
    CHECK(float_is(-std::numeric_limits<float>::infinity(), "-inf"));
    CHECK(float_is(std::numeric_limits<float>::quiet_NaN(), "nan"));
}

NNH_TEST(live_format_round_trips_seeded_floats) {
    std::uint32_t x = 0x12345678u;
    int failures = 0;
    for (int i = 0; i < 1000000 && failures < 5; ++i) {
        x = x * 1664525u + 1013904223u;
        const std::uint32_t bits = x ^ (x >> 13);
        const float v = bits_float(bits);
        if (v != v || v - v != 0) continue; // nan, inf
        if (!round_trips(v)) ++failures;
    }
    CHECK(failures == 0);
}

NNH_TEST(live_format_round_trips_powers_of_two_and_subnormal_edges) {
    for (int e = 1; e < 255; ++e) {
        CHECK(round_trips(bits_float(static_cast<std::uint32_t>(e) << 23)));
        CHECK(round_trips(-bits_float(static_cast<std::uint32_t>(e) << 23)));
    }
    for (std::uint32_t m = 1; m < 64; ++m) CHECK(round_trips(bits_float(m)));
    for (std::uint32_t m = 0x7FFFC0; m <= 0x800040; ++m) CHECK(round_trips(bits_float(m)));
    CHECK(round_trips(bits_float(0x7F7FFFFFu)));
}

NNH_TEST(live_format_integers) {
    char out[kValueTextBytes];
    CHECK(format_int(0, out) == 1 && std::strcmp(out, "0") == 0);
    CHECK(format_int(-2147483647 - 1, out) == 11 && std::strcmp(out, "-2147483648") == 0);
    CHECK(format_int(4294967295LL, out) == 10 && std::strcmp(out, "4294967295") == 0);
    CHECK(format_int(42, out) == 2 && std::strcmp(out, "42") == 0);
}

NNH_TEST(live_format_colours) {
    char out[kValueTextBytes];
    const std::uint32_t c = pack_rgba(0xFF, 0xC0, 0x40, 0xFF);
    CHECK(format_color(c, false, out) == 9 && std::strcmp(out, "#FFC040FF") == 0);
    CHECK(format_color(c, true, out) == 7 && std::strcmp(out, "#FFC040") == 0);
    CHECK(format_color(pack_rgba(1, 2, 0xAB, 0x80), true, out) == 9 && std::strcmp(out, "#0102AB80") == 0);
}

NNH_TEST(live_format_vec2_and_bools) {
    char out[kValueTextBytes * 2];
    CHECK(format_vec2(12.5f, 40.0f, out) == 10 && std::strcmp(out, "(12.5, 40)") == 0);
    CHECK(format_vec2(-0.0f, 1e-7f, out) > 0 && std::strcmp(out, "(-0, 1e-07)") == 0);
    CHECK(format_bool(true, out) == 4 && std::strcmp(out, "true") == 0);
    CHECK(format_bool(false, out) == 5 && std::strcmp(out, "false") == 0);
}

NNH_TEST(live_format_text_is_quoted_and_escaped) {
    char out[kQuotedTextBytes];
    CHECK(format_text("idle", 4, out, sizeof out) == 6 && std::strcmp(out, "\"idle\"") == 0);
    CHECK(format_text("a\"b\\c", 5, out, sizeof out) > 0 && std::strcmp(out, "\"a\\\"b\\\\c\"") == 0);
    CHECK(format_text("x\ny\x7F", 4, out, sizeof out) > 0 && std::strcmp(out, "\"x\\x0Ay\\x7F\"") == 0);
    CHECK(format_text("", 0, out, sizeof out) == 2 && std::strcmp(out, "\"\"") == 0);
}

NNH_TEST(live_format_text_never_overflows_its_buffer) {
    char text[31];
    std::memset(text, '\n', sizeof text);
    char out[kQuotedTextBytes];
    const int n = format_text(text, 31, out, sizeof out);
    CHECK(n == 2 + 31 * 4);
    CHECK(n < static_cast<int>(sizeof out));
    char small[8];
    CHECK(format_text(text, 31, small, sizeof small) < static_cast<int>(sizeof small));
    CHECK(small[std::strlen(small) - 1] == '"');
}
