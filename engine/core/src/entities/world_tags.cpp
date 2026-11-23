// Tags on entities (spec 020 FR-17): up to tags_per_entity 32-bit tag words per record, 0 = empty.

#include "entities/world_internal.hpp"

namespace nnh {
namespace detail {

bool world_has_tag(const WorldHot& w, Entity e, Tag t) {
    const std::uint32_t* r = live_record(w, e);
    if (r == nullptr || t == kNoTag) return false;
    for (int i = 0; i < w.tags; ++i)
        if (r[2 + i] == static_cast<std::uint32_t>(t)) return true;
    return false;
}

} // namespace detail

bool World::add_tag(Entity e, Tag t) {
    if (hot_ == nullptr) {
        detail::world_misuse("use of an empty or moved-from nnh::World");
        return false;
    }
    std::uint32_t* r = detail::live_record(*hot_, e);
    if (r == nullptr) return false;
    if (t == kNoTag) {
        detail::world_misuse("World::add_tag(kNoTag)");
        return false;
    }
    int empty = -1;
    for (int i = 0; i < hot_->tags; ++i) {
        if (r[2 + i] == static_cast<std::uint32_t>(t)) return false;
        if (empty < 0 && r[2 + i] == 0) empty = i;
    }
    if (empty < 0) {
        detail::world_misuse("World::add_tag(): the entity has tags_per_entity tags already");
        return false;
    }
    r[2 + empty] = static_cast<std::uint32_t>(t);
    return true;
}

bool World::remove_tag(Entity e, Tag t) {
    if (hot_ == nullptr || t == kNoTag) return false;
    std::uint32_t* r = detail::live_record(*hot_, e);
    if (r == nullptr) return false;
    for (int i = 0; i < hot_->tags; ++i) {
        if (r[2 + i] != static_cast<std::uint32_t>(t)) continue;
        r[2 + i] = 0;
        return true;
    }
    return false;
}

} // namespace nnh
