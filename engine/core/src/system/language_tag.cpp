// Language tags (language_tag.hpp): splitting and comparing. Pure.

#include "system/language_tag.hpp"

namespace nnh::detail {
namespace {

void copy(char* out, const char* s, int n) {
    for (int i = 0; i < n; ++i) out[i] = s[i];
    out[n] = 0;
}

char lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

} // namespace

bool split_language_tag(const char* tag, LanguageTagParts& out) {
    out = LanguageTagParts{};
    if (!language_tag_well_formed(tag)) return false;
    int n = tag_chars::subtag(tag);
    copy(out.language, tag, n);
    while (tag[n] == '-') {
        const char* part = tag + n + 1;
        const int m = tag_chars::subtag(part);
        copy(m == 4 ? out.script : out.region, part, m);
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
