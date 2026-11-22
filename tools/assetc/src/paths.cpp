// '/'-separated path helpers (see paths.hpp).

#include "paths.hpp"

#include <cctype>
#include <vector>

namespace nnh::assetc {

namespace {

/// "/" or "X:/" at the start of a path whose separators are already '/', else "".
std::string root_of(const std::string& path) {
    if (!path.empty() && path[0] == '/') return "/";
    if (path.size() >= 3 && path[1] == ':' && path[2] == '/') return path.substr(0, 3);
    return "";
}

} // namespace

bool is_absolute_path(const std::string& path) {
    if (!path.empty() && (path[0] == '/' || path[0] == '\\')) return true;
    return path.size() >= 3 && path[1] == ':' && (path[2] == '/' || path[2] == '\\');
}

std::string normalize_path(const std::string& path) {
    std::string slashed = path;
    for (char& c : slashed) {
        if (c == '\\') c = '/';
    }
    const std::string root = root_of(slashed);
    std::vector<std::string> parts;
    std::size_t at = root.size();
    while (at <= slashed.size()) {
        std::size_t end = slashed.find('/', at);
        if (end == std::string::npos) end = slashed.size();
        const std::string part = slashed.substr(at, end - at);
        if (part == "..") {
            if (!parts.empty() && parts.back() != "..") parts.pop_back();
            else if (root.empty()) parts.push_back(part); // a relative path may start above its folder
        } else if (!part.empty() && part != ".") {
            parts.push_back(part);
        }
        at = end + 1;
    }
    std::string out = root;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) out += '/';
        out += parts[i];
    }
    return out;
}

std::string parent_path(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) return "";
    if (slash == 0) return "/";
    return path.substr(0, slash);
}

std::string join_path(const std::string& dir, const std::string& relative) {
    if (is_absolute_path(relative) || dir.empty()) return normalize_path(relative);
    return normalize_path(dir + "/" + relative);
}

std::string relative_path(const std::string& base, const std::string& path) {
    std::string from = normalize_path(base), to = normalize_path(path);
    const std::string from_root = root_of(from), to_root = root_of(to);
    auto same_root = [&] {
        if (from_root.size() != to_root.size()) return false;
        for (std::size_t i = 0; i < from_root.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(from_root[i])) !=
                std::tolower(static_cast<unsigned char>(to_root[i]))) {
                return false;
            }
        }
        return true;
    };
    if (!same_root()) return to;
    auto split = [](const std::string& p, std::size_t skip) {
        std::vector<std::string> parts;
        for (std::size_t at = skip; at < p.size();) {
            std::size_t end = p.find('/', at);
            if (end == std::string::npos) end = p.size();
            parts.push_back(p.substr(at, end - at));
            at = end + 1;
        }
        return parts;
    };
    const std::vector<std::string> a = split(from, from_root.size()), b = split(to, to_root.size());
    std::size_t common = 0;
    while (common < a.size() && common < b.size() && a[common] == b[common]) ++common;
    std::string out;
    for (std::size_t i = common; i < a.size(); ++i) out += out.empty() ? ".." : "/..";
    for (std::size_t i = common; i < b.size(); ++i) out += (out.empty() ? "" : "/") + b[i];
    return out.empty() ? "." : out;
}

bool path_inside(const std::string& root, const std::string& path, std::string& inside) {
    if (path.size() <= root.size() + 1 || path.compare(0, root.size(), root) != 0 || path[root.size()] != '/') {
        return false;
    }
    inside = path.substr(root.size() + 1);
    return true;
}

} // namespace nnh::assetc
