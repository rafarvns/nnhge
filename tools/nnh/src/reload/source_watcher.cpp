// Watching the source assets folder (see source_watcher.hpp).

#include "reload/source_watcher.hpp"

#include <algorithm>
#include <filesystem>

namespace nnh::tool::reload {
namespace {

namespace fs = std::filesystem;

void add_unique(std::vector<std::string>& v, const std::string& s) {
    if (std::find(v.begin(), v.end(), s) == v.end()) v.push_back(s);
}

} // namespace

SourceListing list_sources(const std::string& folder) {
    SourceListing out;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        SourceStamp s;
        s.size = static_cast<std::uint64_t>(it->file_size(ec));
        s.time = static_cast<std::int64_t>(it->last_write_time(ec).time_since_epoch().count());
        std::string rel = fs::relative(it->path(), folder, ec).generic_string();
        if (!ec) out.emplace(std::move(rel), s);
    }
    return out;
}

void SourceWatcher::reset(SourceListing listing) {
    last_ = std::move(listing);
    burst_.clear();
    gathering_ = false;
}

bool SourceWatcher::poll(SourceListing listing, std::int64_t now_ms) {
    bool moved = false;
    for (const auto& [path, stamp] : listing) {
        const auto it = last_.find(path);
        if (it == last_.end() || !(it->second == stamp)) {
            add_unique(burst_, path);
            moved = true;
        }
    }
    for (const auto& [path, stamp] : last_) {
        if (listing.find(path) == listing.end()) {
            add_unique(burst_, path);
            moved = true;
        }
    }
    last_ = std::move(listing);
    if (moved) {
        gathering_ = true;
        last_change_ = now_ms; // the quiet period starts again
        return false;
    }
    if (!gathering_ || now_ms - last_change_ < kQuietMs) return false;
    gathering_ = false;
    changed_ = burst_;
    std::sort(changed_.begin(), changed_.end());
    burst_.clear();
    return true;
}

} // namespace nnh::tool::reload
