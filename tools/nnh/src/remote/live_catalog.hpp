#pragma once
// The PC's mirror of a game's live values (spec 030 FR-11): built from LiveEntry/LiveRemoved records after a catalogue
// request, looked up by name or hash. Value text both ways (live_text.hpp) uses the same rules as the game. Pure;
// tested in tools/nnh/tests/test_live_catalog.cpp.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "live/live_kinds.hpp"
#include "remote/live_wire.hpp"

namespace nnh::tool::remote {

struct LiveItem {
    std::string name;
    std::uint32_t hash = 0;
    detail::live::Kind kind = detail::live::Kind::None;
    std::uint8_t flags = 0; ///< remote::kEntry*
    std::uint32_t value = 0, def = 0, min = 0, max = 0, step = 0;
    std::vector<std::string> enum_names;

    bool tunable() const { return (flags & detail::remote::kEntryTunable) != 0; }
    bool action() const { return (flags & detail::remote::kEntryAction) != 0; }
    bool watch() const { return (flags & detail::remote::kEntryWatch) != 0; }
};

class LiveCatalog {
public:
    /// A LiveEntry or LiveRemoved payload. @return false for a malformed one (ignored).
    bool add_entry(const std::uint8_t* payload, std::uint32_t length);
    void remove(std::uint32_t hash);
    void clear();

    const LiveItem* find(const std::string& name) const;
    const LiveItem* find(std::uint32_t hash) const;
    /// The items in name order (a group: names starting with "GROUP/").
    std::vector<const LiveItem*> list(const std::string& group = "") const;
    /// The closest name (edit distance <= 3), or "".
    std::string closest(const std::string& name) const;
    /// True when `name` is a group of some item.
    bool is_group(const std::string& name) const;

    bool complete = false; ///< the catalogue's LiveState arrived

private:
    std::vector<LiveItem> items_; ///< sorted by name
};

} // namespace nnh::tool::remote
