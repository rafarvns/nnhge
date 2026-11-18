// Asset path rules (asset_path.hpp).

#include "assets/asset_path.hpp"

namespace nnh::detail {
namespace {

bool portable(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' ||
           c == '_' || c == '.';
}

/// A segment (between slashes) of `length` characters starting at `s`: not empty, not "." or "..".
bool valid_segment(const char* s, std::size_t length) {
    if (length == 0) return false;
    if (length == 1 && s[0] == '.') return false;
    if (length == 2 && s[0] == '.' && s[1] == '.') return false;
    return true;
}

} // namespace

bool valid_asset_path(const char* path) {
    if (path == nullptr || *path == '\0') return false;
    const char* segment = path;
    std::size_t length = 0;
    for (const char* p = path;; ++p) {
        if (static_cast<std::size_t>(p - path) > kMaxAssetPath) return false;
        if (*p == '/' || *p == '\0') {
            if (!valid_segment(segment, static_cast<std::size_t>(p - segment))) return false;
            if (*p == '\0') break;
            segment = p + 1;
            continue;
        }
        if (!portable(*p)) return false;
        ++length;
    }
    return length > 0;
}

} // namespace nnh::detail
