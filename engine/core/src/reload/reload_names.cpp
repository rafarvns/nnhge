// Asset paths and load names for hot reload (see reload_names.hpp).

#include "reload/reload_names.hpp"

#include <cstdio>
#include <cstring>

namespace nnh::detail::reload {
namespace {

/// Characters of ", ... (+N)" (`first`: without the leading ", ").
std::size_t suffix_length(int left, bool first) {
    char text[24];
    const int n = std::snprintf(text, sizeof text, "... (+%d)", left);
    return static_cast<std::size_t>(n) + (first ? 0 : 2);
}

} // namespace

std::size_t load_name_length(const char* path) {
    const std::size_t length = std::strlen(path);
    std::size_t segment = 0; // start of the last segment
    std::size_t dot = length;
    for (std::size_t i = 0; i < length; ++i) {
        if (path[i] == '/') {
            segment = i + 1;
            dot = length;
        } else if (path[i] == '.') {
            dot = i;
        }
    }
    return dot < length && dot > segment ? dot : length; // a leading dot is a name, not an extension
}

bool matches(const char* path_or_name, const char* path) {
    if (path_or_name == nullptr || path == nullptr || path_or_name[0] == '\0') return false;
    if (std::strcmp(path_or_name, path) == 0) return true;
    const std::size_t name = load_name_length(path);
    return name < std::strlen(path) && std::strlen(path_or_name) == name && std::strncmp(path_or_name, path, name) == 0;
}

std::size_t format_list(const char* const* paths, int count, char* out, std::size_t capacity) {
    std::size_t full = 0;
    for (int i = 0; i < count; ++i) full += load_name_length(paths[i]) + (i > 0 ? 2 : 0);
    const bool cut = full > kListLimit;
    std::size_t used = 0;
    int kept = 0;
    for (; kept < count; ++kept) {
        const std::size_t name = load_name_length(paths[kept]);
        const std::size_t sep = kept > 0 ? 2 : 0;
        const std::size_t reserve = cut ? suffix_length(count - kept - 1, false) : 0;
        if (cut && used + sep + name + reserve > kListLimit) break;
        if (used + sep + name + 1 > capacity) break;
        if (sep > 0) {
            out[used++] = ',';
            out[used++] = ' ';
        }
        std::memcpy(out + used, paths[kept], name);
        used += name;
    }
    if (kept < count) {
        const int n = std::snprintf(out + used, capacity - used, "%s... (+%d)", kept > 0 ? ", " : "", count - kept);
        used += static_cast<std::size_t>(n > 0 ? n : 0);
    }
    out[used] = '\0';
    return used;
}

} // namespace nnh::detail::reload
