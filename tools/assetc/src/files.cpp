// Input files from disk or memory (see files.hpp).

#include "files.hpp"

#include <algorithm>
#include <cstdio>

namespace nnh::assetc {

bool DiskFiles::read(const std::string& path, std::size_t max_bytes, std::string& out) {
    out.clear();
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) return false;
    char buffer[64 * 1024];
    bool ok = true;
    while (out.size() < max_bytes) {
        const std::size_t want = std::min(sizeof buffer, max_bytes - out.size());
        const std::size_t got = std::fread(buffer, 1, want, file);
        out.append(buffer, got);
        if (got < want) {
            ok = std::ferror(file) == 0;
            break;
        }
    }
    std::fclose(file);
    return ok;
}

bool MemoryFiles::read(const std::string& path, std::size_t max_bytes, std::string& out) {
    const auto found = files_.find(path);
    if (found == files_.end()) {
        out.clear();
        return false;
    }
    out = found->second.substr(0, max_bytes);
    return true;
}

} // namespace nnh::assetc
