// Flattening a registration (spec 021 FR-2, FR-4): the entries in order, the tags with duplicates dropped, the
// placement member, the hook chain. Pure: works on a plan, writes nothing in the block.

#include "prefabs/prefab_internal.hpp"

namespace nnh::detail {
namespace {

bool fail(Error& error, ErrorCode code, const char* what) {
    error = Error::make(code, "PrefabLibrary::add: %s", what);
    return false;
}

bool merge_entries(int count, PrefabPlan& plan, Error& error) {
    for (int j = 0; j < count; ++j) {
        if (plan.entries >= kMaxPrefabEntries) return fail(error, ErrorCode::LimitReached, "more than 32 entries");
        plan.own[plan.entries++] = static_cast<std::int8_t>(j);
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

bool place(const PrefabBlock&, const PrefabOptions& options, const ComponentValue* values, PrefabPlan& plan,
           Error& error) {
    const Placement& p = options.place;
    if (p.kind == nullptr) return true;
    for (int i = 0; i < plan.entries; ++i) {
        const int j = plan.own[i];
        if (values[j].id != p.kind) continue;
        const int member = p.rect ? static_cast<int>(sizeof(Rect)) : static_cast<int>(sizeof(Vec2));
        if (p.offset + member > values[j].size)
            return fail(error, ErrorCode::InvalidArgument, "the placement member is past its component's end");
        plan.place_entry = static_cast<std::uint8_t>(i);
        plan.place_offset = p.offset;
        plan.place_flags = static_cast<std::uint8_t>((p.rect ? kPlaceRect : 0) | (p.keep_size ? kPlaceKeepSize : 0));
        return true;
    }
    return fail(error, ErrorCode::InvalidArgument, "the placement's kind is not among the entries");
}

} // namespace

bool flatten(const PrefabBlock& b, const PrefabOptions& options, const ComponentValue* values, int count,
             PrefabPlan& plan, Error& error) {
    if (options.base) return fail(error, ErrorCode::InvalidArgument, "variants are not supported yet");
    if (!merge_entries(count, plan, error) || !merge_tags(options.tags, plan, error)) return false;
    if (!place(b, options, values, plan, error)) return false;
    plan.hooks = options.on_spawn ? 1 : 0;
    return true;
}

} // namespace nnh::detail
