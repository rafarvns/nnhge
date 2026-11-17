// Path validation — the gate every file operation goes through (docs/architecture/security.md).

#include "nnh/fs.hpp"

namespace nnh::fs {
namespace {

bool allowed(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' ||
           c == '-';
}

Result<void> reject(const char* why) { return Error::make(ErrorCode::InvalidArgument, "bad path: %s", why); }

} // namespace

Result<void> validate_path(const char* path) {
    if (path == nullptr || path[0] == '\0') return reject("empty");
    std::size_t length = 0;
    int depth = 1;
    std::size_t segment_start = 0;
    for (std::size_t i = 0;; ++i) {
        const char c = path[i];
        if (c == '/' || c == '\0') {
            const std::size_t segment_length = i - segment_start;
            if (segment_length == 0) return reject(i == 0 ? "absolute" : "empty segment");
            const char* segment = path + segment_start;
            if (segment[0] == '.' && (segment_length == 1 || (segment_length == 2 && segment[1] == '.'))) {
                return reject("'.' or '..' segment");
            }
            if (c == '\0') break;
            if (++depth > kMaxPathDepth) return reject("too deep");
            segment_start = i + 1;
        } else if (!allowed(c)) {
            return reject("character not allowed");
        }
        if (++length > kMaxPathLength) return reject("too long");
    }
    return {};
}

} // namespace nnh::fs
