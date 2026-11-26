#pragma once
// Language tags (spec 029 FR-15, FR-22, FR-27): the BCP 47-style codes NNHGE names languages with — 2–3 lowercase
// letters, an optional -Xxxx script (title case), an optional -XX (uppercase) or -999 region; at most 15 bytes. Pure,
// no engine includes: the well-formedness check is inline so the PC platform library (language_pc.cpp) and the `nnh`
// tool use it without linking core; language_tag.cpp splits and compares (localization's matching, nnh-assetc).
// Tests: tests/system/test_language_tag.cpp.

namespace nnh::detail {

inline constexpr int kMaxLanguageTag = 15;

namespace tag_chars {
constexpr bool lower(char c) { return c >= 'a' && c <= 'z'; }
constexpr bool upper(char c) { return c >= 'A' && c <= 'Z'; }
constexpr bool digit(char c) { return c >= '0' && c <= '9'; }
/// Length of the subtag at s[at] (up to '-' or `end`).
constexpr int subtag(const char* s, int at, int end) {
    int n = 0;
    while (at + n < end && s[at + n] != '-') ++n;
    return n;
}
constexpr bool script(const char* s, int n) {
    return n == 4 && upper(s[0]) && lower(s[1]) && lower(s[2]) && lower(s[3]);
}
constexpr bool region(const char* s, int n) {
    return (n == 2 && upper(s[0]) && upper(s[1])) || (n == 3 && digit(s[0]) && digit(s[1]) && digit(s[2]));
}
} // namespace tag_chars

/// The length of `s` if it is at most kMaxLanguageTag bytes, else −1. Reads at most kMaxLanguageTag + 1 bytes.
constexpr int language_tag_length(const char* s) {
    if (s == nullptr) return -1;
    int n = 0;
    while (n <= kMaxLanguageTag && s[n] != 0) ++n;
    return n <= kMaxLanguageTag ? n : -1;
}

/// True for a well-formed code ("en", "pt-BR", "zh-Hant", "es-419", "zh-Hans-CN"); false for nullptr. Reads at most 16
/// bytes. Inline for the PC platform library and the `nnh` tool; core calls language_tag_valid() (one copy).
constexpr bool language_tag_well_formed(const char* s) {
    using namespace tag_chars;
    const int end = language_tag_length(s);
    if (end < 2) return false;
    const int n = subtag(s, 0, end);
    if (n < 2 || n > 3) return false;
    for (int i = 0; i < n; ++i)
        if (!lower(s[i])) return false;
    int at = n;
    bool had_script = false, had_region = false;
    while (at < end) { // s[at] == '-'
        const int m = subtag(s, at + 1, end);
        const char* part = s + at + 1;
        if (!had_script && !had_region && script(part, m)) {
            had_script = true;
        } else if (!had_region && region(part, m)) {
            had_region = true;
        } else {
            return false;
        }
        at += 1 + m;
    }
    return true;
}

/// language_tag_well_formed(), out of line (core's callers share one copy: Debug/Development hello's size).
bool language_tag_valid(const char* s);

/// The parts of a well-formed tag, NUL-terminated (empty when absent).
struct LanguageTagParts {
    char language[4] = {};
    char script[5] = {};
    char region[4] = {};
};

/// Splits a well-formed tag. @return false (parts unspecified) for an ill-formed one.
bool split_language_tag(const char* tag, LanguageTagParts& out);

/// Two tags are the same code, ignoring case ("pt-BR" and "pt-br"). Compares at most 16 bytes; nullptr is no tag.
bool same_language_tag(const char* a, const char* b);

} // namespace nnh::detail
