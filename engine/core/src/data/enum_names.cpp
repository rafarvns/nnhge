// Names of a data enum (enum_names.hpp). Pure.

#include "data/enum_names.hpp"

#include <cstring>

namespace nnh::detail {

namespace {

bool space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

/// The name starting at `p` (leading spaces skipped), up to the next comma, trailing spaces trimmed; `p`
/// moves past the comma (or to the end).
NameView next_name(const char*& p) {
    while (space(*p)) ++p;
    NameView v{p, 0};
    while (*p != '\0' && *p != ',') ++p;
    v.length = static_cast<int>(p - v.text);
    while (v.length > 0 && space(v.text[v.length - 1])) --v.length;
    if (*p == ',') ++p;
    return v;
}

} // namespace

NameView enum_name(const char* names, int index) {
    if (index < 0) return {};
    const char* p = names;
    for (int i = 0; *p != '\0'; ++i) {
        const NameView v = next_name(p);
        if (i == index) return v;
    }
    return {};
}

int enum_index(const char* names, const char* name, int length) {
    if (length <= 0) return -1;
    const char* p = names;
    for (int i = 0; *p != '\0'; ++i) {
        const NameView v = next_name(p);
        if (v.length == length && std::memcmp(v.text, name, static_cast<std::size_t>(length)) == 0) return i;
    }
    return -1;
}

} // namespace nnh::detail
