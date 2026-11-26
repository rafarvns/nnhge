// Language tags (language_tag.hpp): splitting and comparing. Pure.

#include "system/language_tag.hpp"

namespace nnh::detail {
namespace {

template <int N>
void copy(char (&out)[N], const char* s, int n) {
    if (n > N - 1) n = N - 1; // never for a well-formed tag
    for (int i = 0; i < n; ++i) out[i] = s[i];
    out[n] = 0;
}

char lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

} // namespace

bool language_tag_valid(const char* s) { return language_tag_well_formed(s); }

bool split_language_tag(const char* tag, LanguageTagParts& out) {
    out = LanguageTagParts{};
    if (!language_tag_well_formed(tag)) return false;
    const int end = language_tag_length(tag);
    int n = tag_chars::subtag(tag, 0, end);
    copy(out.language, tag, n);
    while (n < end) {
        const int m = tag_chars::subtag(tag, n + 1, end);
        if (m == 4) copy(out.script, tag + n + 1, m);
        else copy(out.region, tag + n + 1, m);
        n += 1 + m;
    }
    return true;
}

bool same_language_tag(const char* a, const char* b) {
    if (a == nullptr || b == nullptr) return false;
    for (int i = 0; i <= kMaxLanguageTag; ++i) {
        if (lower(a[i]) != lower(b[i])) return false;
        if (a[i] == 0) return true;
    }
    return false;
}

} // namespace nnh::detail
