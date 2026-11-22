// Applying a bindings blob (spec 017 FR-21): validated whole first (nothing changes on a damaged, too-new or
// other-version blob); then every remappable slot returns to its default and each entry that fits is applied.
// Unknown actions, slots out of range, parts or kinds that don't fit, data out of range, unknown flags, touch,
// locked slots and repeated keys are skipped and counted, with one warning. Never asserts on content.

#include "actions/actions_internal.hpp"
#include "actions/bindings_format.hpp"

namespace nnh {
namespace detail {
namespace {

namespace fmt = bindings;

ActionRec* by_hash(ActionBlock& b, std::uint32_t hash) {
    for (int i = 0; i < b.declared; ++i) {
        if (b.actions[b.order[i]].hash == hash) return &b.actions[b.order[i]];
    }
    return nullptr;
}

bool whole_fits(const ActionRec& r, const fmt::Entry& e, PackedSource& out) {
    if ((e.flags & ~(fmt::kInvertX | fmt::kInvertY)) != 0 || e.kind > static_cast<std::uint8_t>(Source::Kind::Touch)) {
        return false;
    }
    out.kind = static_cast<Source::Kind>(e.kind);
    for (int i = 0; i < 4; ++i) out.d[i] = e.data[i];
    return out.kind != Source::Kind::Touch && source_fits(r.kind, out.kind) && source_data_valid(out);
}

bool part_entry_fits(const ActionRec& r, const fmt::Entry& e) {
    if (e.part > static_cast<std::uint8_t>(Part::Right) || !part_fits(r.kind, static_cast<Part>(e.part))) return false;
    if (e.flags != 0 || e.data[1] != 0 || e.data[2] != 0 || e.data[3] != 0) return false;
    if (e.kind == static_cast<std::uint8_t>(Source::Kind::None)) return e.data[0] == 0;
    return e.kind == static_cast<std::uint8_t>(Source::Kind::Button) && e.data[0] < 14;
}

void set_part(SlotRec& s, ActionKind kind, Part part, const fmt::Entry& e) {
    const Source::Kind composite = kind == ActionKind::Axis ? Source::Kind::Buttons2 : Source::Kind::Buttons4;
    if (s.cur.kind != composite) {
        s.cur = PackedSource{composite, {Source::kNoButton, Source::kNoButton, 0, 0}};
        if (composite == Source::Kind::Buttons4) s.cur.d[2] = s.cur.d[3] = Source::kNoButton;
    }
    s.cur.d[part_index(part)] = e.kind == static_cast<std::uint8_t>(Source::Kind::Button) ? e.data[0] : Source::kNoButton;
}

/// Applies one entry; false when it is skipped. `seen` holds the (slot, part) keys already applied per action.
bool apply(ActionBlock& b, const fmt::Entry& e, std::uint8_t (*seen)[kMaxSlots]) {
    ActionRec* r = by_hash(b, e.hash);
    if (r == nullptr || e.slot >= kMaxSlots || e.part > static_cast<std::uint8_t>(Part::Right)) return false;
    SlotRec& s = r->slots[e.slot];
    if ((s.flags & slot_flag::kLocked) != 0) return false;
    std::uint8_t& keys = seen[r - b.actions][e.slot];
    if ((keys & (1u << e.part)) != 0) return false;
    PackedSource whole;
    if (e.part == 0 ? !whole_fits(*r, e, whole) : !part_entry_fits(*r, e)) return false;
    keys = static_cast<std::uint8_t>(keys | (1u << e.part));
    if (e.part == 0) {
        s.cur = whole;
        s.flags = static_cast<std::uint8_t>((s.flags & ~(slot_flag::kInvertX | slot_flag::kInvertY)) |
                                            ((e.flags & fmt::kInvertX) != 0 ? slot_flag::kInvertX : 0) |
                                            ((e.flags & fmt::kInvertY) != 0 ? slot_flag::kInvertY : 0));
    } else {
        set_part(s, r->kind, static_cast<Part>(e.part), e);
    }
    slot_changed(s, b.caps);
    return true;
}

} // namespace
} // namespace detail

Result<LoadReport> ActionSet::load_from(const void* data, std::size_t size, std::uint32_t game_version) {
    if (block_ == nullptr) return Error::make(ErrorCode::InvalidArgument, "empty action set");
    int count = 0;
    std::uint32_t version = 0;
    switch (detail::bindings::validate(data, size, count, version)) {
    case detail::bindings::Check::Ok: break;
    case detail::bindings::Check::Unsupported:
        return Error::make(ErrorCode::Unsupported, "bindings saved by a newer engine");
    case detail::bindings::Check::Corrupted: return Error::make(ErrorCode::Corrupted, "bindings data is damaged");
    }
    LoadReport report;
    report.found = true;
    if (version != game_version) {
        report.version_ignored = true;
        return report;
    }
    reset_all();
    std::uint8_t seen[kMaxActions][kMaxSlots] = {};
    const auto* entries = static_cast<const std::uint8_t*>(data) + detail::bindings::kHeaderSize;
    for (int i = 0; i < count; ++i) {
        const detail::bindings::Entry e = detail::bindings::decode_entry(entries + i * detail::bindings::kEntrySize);
        if (detail::apply(*block_, e, seen)) {
            ++report.applied;
        } else {
            ++report.skipped;
        }
    }
    if (report.skipped > 0) {
        NNH_LOG_WARN(actions, "bindings: %d entries skipped (unknown or unfitting), %d applied", report.skipped,
                     report.applied);
    }
    return report;
}

} // namespace nnh
