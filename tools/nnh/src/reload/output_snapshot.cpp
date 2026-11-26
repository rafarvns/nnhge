// What the game has (see output_snapshot.hpp).

#include "reload/output_snapshot.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>

#include "reload/reload_overlay.hpp" // path_hash, the game's per-path hash

namespace nnh::tool::reload {
namespace {

namespace fs = std::filesystem;

/// Every regular file under `folder`: relative '/' path → size and time (no hash yet).
std::map<std::string, OutputFile> list(const std::string& folder) {
    std::map<std::string, OutputFile> out;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        OutputFile f;
        f.size = static_cast<std::uint64_t>(it->file_size(ec));
        f.time = static_cast<std::int64_t>(it->last_write_time(ec).time_since_epoch().count());
        std::string rel = fs::relative(it->path(), folder, ec).generic_string();
        if (!ec) out.emplace(std::move(rel), f);
    }
    return out;
}

int index_rank(const std::string& path) {
    if (path == ".nnh_assets") return 3;
    if (path == ".nnh_loc") return 2;
    if (path == ".nnh_data") return 1;
    return 0;
}

} // namespace

std::uint64_t file_hash(const std::string& path, bool& ok) {
    ok = false;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return 0;
    std::uint64_t h = 0xCBF29CE484222325ull;
    unsigned char buffer[64 * 1024];
    std::size_t n = 0;
    while ((n = std::fread(buffer, 1, sizeof buffer, f)) > 0) {
        for (std::size_t i = 0; i < n; ++i) {
            h ^= buffer[i];
            h *= 0x100000001B3ull;
        }
    }
    ok = std::ferror(f) == 0;
    std::fclose(f);
    return h;
}

bool batch_order(const OutputChange& a, const OutputChange& b) {
    const int ra = index_rank(a.path), rb = index_rank(b.path);
    return ra != rb ? ra < rb : a.path < b.path;
}

void OutputSnapshot::scan(const std::string& folder) {
    files_ = list(folder);
    for (auto& [path, f] : files_) {
        bool ok = false;
        f.hash = file_hash(folder + "/" + path, ok);
    }
}

std::vector<OutputChange> OutputSnapshot::diff(const std::string& folder) const {
    std::vector<OutputChange> changes;
    std::map<std::string, OutputFile> now = list(folder);
    for (auto& [path, f] : now) {
        const auto it = files_.find(path);
        if (it != files_.end() && it->second.size == f.size && it->second.time == f.time) continue; // untouched
        bool ok = false;
        f.hash = file_hash(folder + "/" + path, ok);
        if (!ok) continue; // being written: the next cycle sees it
        if (it != files_.end() && it->second.size == f.size && it->second.hash == f.hash) continue; // same bytes
        changes.push_back(OutputChange{path, false, f});
    }
    for (const auto& [path, f] : files_) {
        if (now.find(path) == now.end()) changes.push_back(OutputChange{path, true, {}});
    }
    std::sort(changes.begin(), changes.end(), batch_order);
    return changes;
}

void OutputSnapshot::accept(const std::vector<OutputChange>& changes) {
    for (const OutputChange& c : changes) {
        if (c.removed) files_.erase(c.path);
        else files_[c.path] = c.file;
    }
}

void OutputSnapshot::forget(const std::string& path) { files_.erase(path); }

std::vector<std::string> OutputSnapshot::hash_collisions() const {
    std::map<std::uint32_t, std::string> seen;
    std::vector<std::string> out;
    for (const auto& [path, f] : files_) {
        const std::uint32_t h = nnh::detail::reload::path_hash(path.c_str());
        auto [it, fresh] = seen.emplace(h, path);
        if (!fresh) out.push_back(it->second + " and " + path);
    }
    return out;
}

} // namespace nnh::tool::reload
