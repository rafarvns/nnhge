// Live value names (see live_names.hpp). Pure, shared with tools/nnh.

#include "live/live_names.hpp"

namespace nnh::detail::live {

live_name::Error check_name(const char* text) {
    std::size_t n = 0;
    while (text != nullptr && n <= static_cast<std::size_t>(nnh::live::kMaxNameBytes) && text[n] != '\0') ++n;
    return text == nullptr ? live_name::Error::EmptyPart : live_name::check(text, n);
}

const char* name_error_text(live_name::Error error) {
    switch (error) {
    case live_name::Error::None: return "";
    case live_name::Error::TooLong: return "is longer than 31 bytes";
    case live_name::Error::BadCharacter: return "has a character other than letters, digits, '_', '-' and '/'";
    case live_name::Error::EmptyPart: return "has an empty part";
    case live_name::Error::TooDeep: return "has more than 3 group levels";
    }
    return "";
}

int group_depth(const char* text, std::size_t length) {
    int depth = 0;
    for (std::size_t i = 0; i < length; ++i) depth += text[i] == '/' ? 1 : 0;
    return depth;
}

std::size_t group_length(const char* text, std::size_t length, int level) {
    int seen = 0;
    for (std::size_t i = 0; i < length; ++i) {
        if (text[i] != '/') continue;
        if (seen == level) return i;
        ++seen;
    }
    return 0;
}

const char* leaf(const char* text, std::size_t length) {
    std::size_t start = 0;
    for (std::size_t i = 0; i < length; ++i) {
        if (text[i] == '/') start = i + 1;
    }
    return text + start;
}

std::uint32_t group_hash(const char* text, std::size_t length, int level) {
    const std::size_t n = group_length(text, length, level);
    return n == 0 ? 0 : fnv1a32(text, n);
}

bool in_group(const char* text, std::size_t length, const char* prefix, std::size_t prefix_length) {
    if (prefix_length == 0 || prefix_length >= length || text[prefix_length] != '/') return false;
    for (std::size_t i = 0; i < prefix_length; ++i) {
        if (text[i] != prefix[i]) return false;
    }
    return true;
}

} // namespace nnh::detail::live
