// UTF-8 decoding for text (text/utf8.hpp, spec 005 FR-2): never crashes, never reads past the end.

#include <cstdint>
#include <initializer_list>

#include "test.hpp"
#include "text/utf8.hpp"

using nnh::detail::kReplacementChar;
using nnh::detail::next_codepoint;

namespace {

/// Decodes all of `s` into `out`; returns how many code points.
int decode_all(const char* s, char32_t* out, int max) {
    int n = 0;
    while (n < max) {
        const char32_t c = next_codepoint(s);
        if (c == 0) break;
        out[n++] = c;
    }
    return n;
}

} // namespace

NNH_TEST(utf8_decodes_ascii) {
    char32_t c[4];
    CHECK(decode_all("Ab1", c, 4) == 3);
    CHECK(c[0] == U'A' && c[1] == U'b' && c[2] == U'1');
}

NNH_TEST(utf8_decodes_two_byte_latin_letters) {
    char32_t c[8];
    CHECK(decode_all("ação", c, 8) == 4);
    CHECK(c[0] == U'a' && c[1] == U'ç' && c[2] == U'ã' && c[3] == U'o');
}

NNH_TEST(utf8_decodes_three_and_four_byte_sequences) {
    char32_t c[4];
    CHECK(decode_all("\xE2\x82\xAC\xF0\x9F\x98\x80", c, 4) == 2); // euro sign, emoji
    CHECK(c[0] == 0x20AC && c[1] == 0x1F600);
}

NNH_TEST(utf8_end_of_string_returns_zero_and_stays) {
    const char* s = "";
    CHECK(next_codepoint(s) == 0);
    CHECK(next_codepoint(s) == 0);
    CHECK(*s == '\0');
}

NNH_TEST(utf8_invalid_lead_bytes_become_replacement_one_byte_each) {
    char32_t c[4];
    CHECK(decode_all("\x80\xFFz", c, 4) == 3);
    CHECK(c[0] == kReplacementChar && c[1] == kReplacementChar && c[2] == U'z');
}

NNH_TEST(utf8_truncated_sequence_does_not_skip_the_end) {
    char32_t c[4];
    CHECK(decode_all("a\xC3", c, 4) == 2); // the string ends inside a 2-byte sequence
    CHECK(c[0] == U'a' && c[1] == kReplacementChar);
    CHECK(decode_all("\xE2\x82z", c, 4) == 2); // 3-byte sequence cut by a normal character
    CHECK(c[0] == kReplacementChar && c[1] == U'z');
}

NNH_TEST(utf8_overlong_and_surrogate_sequences_are_rejected) {
    char32_t c[8];
    CHECK(decode_all("\xC0\xAF", c, 8) >= 1); // overlong '/'
    CHECK(c[0] == kReplacementChar);
    CHECK(decode_all("\xE0\x80\xAF", c, 8) >= 1); // overlong 3-byte
    CHECK(c[0] == kReplacementChar);
    CHECK(decode_all("\xED\xA0\x80", c, 8) >= 1); // UTF-16 surrogate
    CHECK(c[0] == kReplacementChar);
    CHECK(decode_all("\xF4\x90\x80\x80", c, 8) >= 1); // above U+10FFFF
    CHECK(c[0] == kReplacementChar);
}

NNH_TEST(utf8_a_bad_byte_does_not_swallow_the_next_character) {
    char32_t c[4];
    CHECK(decode_all("\xC3" "A", c, 4) == 2); // lead byte followed by a non-continuation
    CHECK(c[0] == kReplacementChar && c[1] == U'A');
}

NNH_TEST(utf8_valid_accepts_well_formed_text_of_a_given_length) {
    using nnh::detail::utf8_valid;
    CHECK(utf8_valid("", 0));
    CHECK(utf8_valid(nullptr, 0));
    CHECK(utf8_valid("abc", 3));
    CHECK(utf8_valid("a\xC3\xA7\xE2\x82\xAC\xF0\x9F\x98\x80", 10)); // a, ç, €, emoji
    CHECK(utf8_valid("a\0b", 3));                                   // NUL is valid UTF-8 (callers refuse it)
    CHECK(utf8_valid("\xEF\xBF\xBF\xF4\x8F\xBF\xBF", 7));          // U+FFFF, U+10FFFF
}

NNH_TEST(utf8_valid_refuses_bad_sequences) {
    using nnh::detail::utf8_valid;
    CHECK(!utf8_valid("\x80", 1));             // continuation without a lead
    CHECK(!utf8_valid("\xC3", 1));             // cut at the end of the slice
    CHECK(!utf8_valid("\xC3\xA7", 1));         // cut by the length, not by a NUL
    CHECK(!utf8_valid("\xC3" "A", 2));         // lead byte followed by a non-continuation
    CHECK(!utf8_valid("\xC0\xAF", 2));         // overlong 2-byte
    CHECK(!utf8_valid("\xE0\x80\xAF", 3));     // overlong 3-byte
    CHECK(!utf8_valid("\xED\xA0\x80", 3));     // UTF-16 surrogate
    CHECK(!utf8_valid("\xF0\x80\x80\x80", 4)); // overlong 4-byte
    CHECK(!utf8_valid("\xF4\x90\x80\x80", 4)); // above U+10FFFF
    CHECK(!utf8_valid("\xF5\x80\x80\x80", 4)); // impossible lead
    CHECK(!utf8_valid("\xFF", 1));
}

NNH_TEST(utf8_random_bytes_always_end_at_the_terminator) {
    // Property: for any bytes, decoding visits the string once, ends exactly at '\0' and never past it.
    std::uint32_t seed = 2026;
    char buffer[33];
    for (int round = 0; round < 2000; ++round) {
        const int length = static_cast<int>(seed % 32);
        for (int i = 0; i < length; ++i) {
            seed = seed * 1664525u + 1013904223u;
            buffer[i] = static_cast<char>((seed >> 24) | 1); // never 0 inside the string
        }
        buffer[length] = '\0';
        const char* p = buffer;
        int steps = 0;
        while (next_codepoint(p) != 0 && steps <= length) ++steps;
        CHECK(p == buffer + length);
        CHECK(steps <= length);
        seed = seed * 1664525u + 1013904223u;
    }
}

// Spec 029 FR-12: every sequence length at its edges, every overlong length, surrogates, the limit U+10FFFF, sequences
// truncated at the NUL and continuation bytes alone (maximal subparts: one replacement each, resumed at the next byte that
// can't continue the sequence).
NNH_TEST(utf8_decodes_the_first_and_last_code_point_of_every_length) {
    char32_t c[8];
    CHECK(decode_all("\x01\x7F", c, 8) == 2 && c[0] == 0x01 && c[1] == 0x7F);
    CHECK(decode_all("\xC2\x80\xDF\xBF", c, 8) == 2 && c[0] == 0x80 && c[1] == 0x7FF);
    CHECK(decode_all("\xE0\xA0\x80\xEF\xBF\xBF", c, 8) == 2 && c[0] == 0x800 && c[1] == 0xFFFF);
    CHECK(decode_all("\xF0\x90\x80\x80\xF4\x8F\xBF\xBF", c, 8) == 2 && c[0] == 0x10000 && c[1] == 0x10FFFF);
    CHECK(decode_all("\xED\x9F\xBF\xEE\x80\x80", c, 8) == 2 && c[0] == 0xD7FF && c[1] == 0xE000); // around surrogates
}

NNH_TEST(utf8_refuses_u110000_and_every_overlong_length) {
    char32_t c[8];
    CHECK(decode_all("\xF4\x90\x80\x80", c, 8) == 4 && c[0] == kReplacementChar); // U+110000: F4 then 3 strays
    CHECK(decode_all("\xC1\xBF", c, 8) == 2 && c[0] == kReplacementChar && c[1] == kReplacementChar);
    CHECK(decode_all("\xE0\x9F\xBF", c, 8) == 3 && c[0] == kReplacementChar);
    CHECK(decode_all("\xF0\x8F\xBF\xBF", c, 8) == 4 && c[0] == kReplacementChar);
    CHECK(decode_all("\xED\xBF\xBF", c, 8) == 3 && c[0] == kReplacementChar); // low surrogate U+DFFF
}

NNH_TEST(utf8_sequences_cut_by_the_nul_stop_there) {
    for (const char* cut : {"\xE2", "\xE2\x82", "\xF0", "\xF0\x9F", "\xF0\x9F\x98"}) {
        const char* p = cut;
        CHECK(next_codepoint(p) == kReplacementChar);
        CHECK(*p == '\0');
        CHECK(next_codepoint(p) == 0);
    }
}

NNH_TEST(utf8_continuation_bytes_alone_are_one_replacement_each) {
    char32_t c[8];
    CHECK(decode_all("\x80\xBF\x80" "a", c, 8) == 4);
    CHECK(c[0] == kReplacementChar && c[1] == kReplacementChar && c[2] == kReplacementChar && c[3] == U'a');
    CHECK(decode_all("\xE2\x82\xAC\xAC", c, 8) == 2 && c[0] == 0x20AC && c[1] == kReplacementChar);
}

NNH_TEST(utf8_fast_decoding_equals_next_codepoint) {
    // T10.3: next_codepoint_fast() decodes ASCII and common 3-byte sequences inline; every lead byte with every second
    // and third byte (and a terminator after each), decoded to the end by both
    int wrong = 0;
    for (int a = 0; a < 256; ++a) {
        for (int b = 0; b < 256; ++b) {
            for (int c = 0; c < 256; c += (a >= 0xE0 && a <= 0xF4) ? 1 : 51) {
                const char s[5] = {static_cast<char>(a), static_cast<char>(b), static_cast<char>(c), static_cast<char>(0x80), 0};
                const char* p = s;
                const char* q = s;
                for (int i = 0; i < 5; ++i) {
                    const char32_t x = next_codepoint(p), y = nnh::detail::next_codepoint_fast(q);
                    wrong += x != y || p != q ? 1 : 0;
                }
            }
        }
    }
    CHECK(wrong == 0);
}
