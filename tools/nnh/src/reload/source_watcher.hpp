#pragma once
// Watching the game's source assets folder (spec 031 FR-1, plan question 3): polled every kPollMs (size and time of every
// file, temporary files included); a change starts a quiet period of kQuietMs that restarts on every further change, so
// an editor's temporary-file-and-rename save or a Tiled save of a map and its tileset becomes one reload. Pure: the
// listing and the clock are given. Tests: tools/nnh/tests/test_source_watcher.cpp.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace nnh::tool::reload {

inline constexpr int kPollMs = 100;
inline constexpr int kQuietMs = 250;

struct SourceStamp {
    std::uint64_t size = 0;
    std::int64_t time = 0;
    bool operator==(const SourceStamp& o) const { return size == o.size && time == o.time; }
};
using SourceListing = std::map<std::string, SourceStamp>; ///< path relative to the folder → stamp

/// Every regular file under `folder` (an empty listing if it can't be read).
SourceListing list_sources(const std::string& folder);

class SourceWatcher {
public:
    /// The listing a reload starts from (the session's start, or right after a cycle's build).
    void reset(SourceListing listing);
    /// A poll at `now_ms`. @return true once per burst: changes were seen and none for kQuietMs since the last one.
    bool poll(SourceListing listing, std::int64_t now_ms);
    /// A burst is being gathered (a change seen, the quiet period not over).
    bool gathering() const { return gathering_; }
    /// The paths changed in the last burst (added, removed or rewritten), sorted.
    const std::vector<std::string>& changed() const { return changed_; }

private:
    SourceListing last_;
    std::vector<std::string> changed_;
    std::vector<std::string> burst_;
    bool gathering_ = false;
    std::int64_t last_change_ = 0;
};

} // namespace nnh::tool::reload
