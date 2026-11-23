// Flattening a registration (spec 021 FR-2, FR-4): the entries in order, the tags with duplicates dropped, the
// placement member, the hook chain. Pure: works on a plan, writes nothing in the block.

#include "prefabs/prefab_internal.hpp"

namespace nnh::detail {
namespace {

bool fail(Error& error, ErrorCode code, const char* what) {
    error = Error::make(code, "PrefabLibrary::add: %s", what);
    return false;
}

/// The base's entries in its order, each replaced in place by an own value of its kind; the other own values appended
/// in call order.
bool merge_entries(const PrefabBlock& b, const PrefabRecord* base, const ComponentValue* values, int count,
                   PrefabPlan& plan, Error& error) {
    const int inherited = base != nullptr ? base->entries : 0;
    for (int i = 0; i < inherited; ++i) plan.own[i] = -1;
    plan.entries = inherited;
    for (int j = 0; j < count; ++j) {
        int at = -1;
        for (int i = 0; i < inherited && at < 0; ++i)
            if (b.entries[base->first + i].id == values[j].id) at = i;
        if (at < 0) {
            if (plan.entries >= kMaxPrefabEntries) return fail(error, ErrorCode::LimitReached, "more than 32 entries");
            at = plan.entries++;
        }
        plan.own[at] = static_cast<std::int8_t>(j);
    }
    return true;
}

bool merge_tags(const Tag* own, PrefabPlan& plan, Error& error) {
    bool ended = false;
    for (int i = 0; i < kMaxPrefabTags; ++i) {
        const Tag t = own[i];
        if (t == kNoTag) {
            ended = true;
            continue;
        }
        if (ended) return fail(error, ErrorCode::InvalidArgument, "kNoTag before another tag");
        bool present = false;
        for (int k = 0; k < plan.tag_count; ++k) present = present || plan.tags[k] == t;
        if (present) continue;
        if (plan.tag_count >= kMaxPrefabTags) return fail(error, ErrorCode::LimitReached, "more than 8 tags");
        plan.tags[plan.tag_count++] = t;
    }
    return true;
}

bool place(const PrefabBlock& b, const PrefabRecord* base, const PrefabOptions& options,
           const ComponentValue* values, PrefabPlan& plan, Error& error) {
    const Placement& p = options.place;
    if (p.kind == nullptr) { // inherited (the base's entries keep their positions)
        if (base != nullptr) {
            plan.place_entry = base->place_entry;
            plan.place_flags = base->place_flags;
            plan.place_offset = base->place_offset;
        }
        return true;
    }
    for (int i = 0; i < plan.entries; ++i) {
        const int j = plan.own[i];
        const TypeId id = j >= 0 ? values[j].id : b.entries[base->first + i].id;
        if (id != p.kind) continue;
        const int member = p.rect ? static_cast<int>(sizeof(Rect)) : static_cast<int>(sizeof(Vec2));
        if (j >= 0 && p.offset + member > values[j].size) // an inherited value's size isn't stored: place() is trusted
            return fail(error, ErrorCode::InvalidArgument, "the placement member is past its component's end");
        plan.place_entry = static_cast<std::uint8_t>(i);
        plan.place_offset = p.offset;
        plan.place_flags = static_cast<std::uint8_t>((p.rect ? kPlaceRect : 0) | (p.keep_size ? kPlaceKeepSize : 0));
        return true;
    }
    return fail(error, ErrorCode::InvalidArgument, "the placement's kind is not among the entries");
}

/// The hook chain: the base's hooks, then the own one (at most 8 in all); the link to the nearest hooked base.
bool chain(const PrefabRecord* base, int base_index, const PrefabOptions& options, PrefabPlan& plan, Error& error) {
    const int hooks = (base != nullptr ? base->hooks : 0) + (options.on_spawn ? 1 : 0);
    if (hooks > kMaxPrefabHooks) return fail(error, ErrorCode::LimitReached, "more than 8 hooks in the chain");
    plan.hooks = static_cast<std::uint8_t>(hooks);
    if (base != nullptr) plan.hook_base = base->hook ? static_cast<std::uint16_t>(base_index) : base->hook_base;
    return true;
}

} // namespace

bool flatten(const PrefabBlock& b, const PrefabOptions& options, const ComponentValue* values, int count,
             PrefabPlan& plan, Error& error) {
    const PrefabRecord* base = nullptr;
    if (options.base) {
        base = record_of(&b, options.base);
        if (base == nullptr) return fail(error, ErrorCode::InvalidArgument, "the base is not a prefab of this library");
        plan.base = options.base.slot();
        for (int i = 0; i < base->tag_count; ++i) plan.tags[i] = base->tags[i];
        plan.tag_count = base->tag_count;
    }
    return merge_entries(b, base, values, count, plan, error) && merge_tags(options.tags, plan, error) &&
           place(b, base, options, values, plan, error) && chain(base, plan.base, options, plan, error);
}

} // namespace nnh::detail
