// Saving and loading player bindings (spec 017 FR-20, FR-21): only what differs from the defaults, keyed by
// action name hash, slot and part (bindings_format.hpp). Loading validates the whole blob before anything changes,
// then returns every remappable slot to its default and applies the entries that fit, skipping and counting the
// rest. Never asserts on content; allocates nothing (save(path)/load(path) use the set's file buffer).

#include "actions/actions_internal.hpp"
#include "actions/bindings_format.hpp"
#include "nnh/fs.hpp"

namespace nnh {
namespace detail {
namespace {

namespace fmt = bindings;
constexpr std::uint16_t kSaveVersion = 1; // nnh::save's own header version

std::uint8_t inverts(std::uint8_t flags, bool defaults) {
    const std::uint8_t x = defaults ? slot_flag::kDefInvertX : slot_flag::kInvertX;
    const std::uint8_t y = defaults ? slot_flag::kDefInvertY : slot_flag::kInvertY;
    return static_cast<std::uint8_t>(((flags & x) != 0 ? fmt::kInvertX : 0) | ((flags & y) != 0 ? fmt::kInvertY : 0));
}

/// The one composite part where `cur` and `def` differ, or -1 (none, several, or not both the same composite).
int single_changed_part(const PackedSource& cur, const PackedSource& def) {
    if (cur.kind != def.kind || (cur.kind != Source::Kind::Buttons2 && cur.kind != Source::Kind::Buttons4)) return -1;
    int changed = -1;
    for (int i = 0; i < 4; ++i) {
        if (cur.d[i] == def.d[i]) continue;
        if (changed >= 0) return -1;
        changed = i;
    }
    return changed;
}

fmt::Entry entry_for(const ActionRec& r, int s) {
    const SlotRec& slot = r.slots[s];
    fmt::Entry e;
    e.hash = r.hash;
    e.slot = static_cast<std::uint8_t>(s);
    const std::uint8_t inv = inverts(slot.flags, false);
    const int part = single_changed_part(slot.cur, slot.def);
    if (part >= 0 && inv == inverts(slot.flags, true)) {
        e.part = static_cast<std::uint8_t>(slot.cur.kind == Source::Kind::Buttons2 ? 1 + part : 3 + part);
        const std::uint8_t button = slot.cur.d[part];
        e.kind = static_cast<std::uint8_t>(button == Source::kNoButton ? Source::Kind::None : Source::Kind::Button);
        if (button != Source::kNoButton) e.data[0] = button;
        return e;
    }
    e.kind = static_cast<std::uint8_t>(slot.cur.kind);
    e.flags = inv;
    for (int i = 0; i < 4; ++i) e.data[i] = slot.cur.d[i];
    return e;
}

bool differs(const SlotRec& s) {
    return (s.flags & slot_flag::kLocked) == 0 && (!(s.cur == s.def) || inverts(s.flags, false) != inverts(s.flags, true));
}

/// Writes the entries (when `out` is not null) and returns how many there are.
int write_entries(const ActionBlock& b, std::uint8_t* out) {
    int n = 0;
    for (int i = 0; i < b.declared; ++i) {
        const ActionRec& r = b.actions[b.order[i]];
        for (int s = 0; s < kMaxSlots; ++s) {
            if (!differs(r.slots[s])) continue;
            if (out != nullptr) fmt::encode_entry(entry_for(r, s), out + n * fmt::kEntrySize);
            ++n;
        }
    }
    return n;
}

Result<std::size_t> encode(const ActionBlock* b, void* buffer, std::size_t capacity, std::uint32_t version) {
    if (b == nullptr) return Error::make(ErrorCode::InvalidArgument, "empty action set");
    const int n = write_entries(*b, nullptr);
    const std::size_t size = fmt::kHeaderSize + fmt::kEntrySize * static_cast<std::size_t>(n);
    if (buffer == nullptr || capacity < size) return Error::make(ErrorCode::LimitReached, "bindings need %zu bytes", size);
    auto* blob = static_cast<std::uint8_t*>(buffer);
    write_entries(*b, blob + fmt::kHeaderSize);
    fmt::sort_entries(blob + fmt::kHeaderSize, n);
    fmt::finish_blob(blob, n, version);
    return size;
}

} // namespace
} // namespace detail

Result<std::size_t> ActionSet::save_to(void* buffer, std::size_t capacity, std::uint32_t game_version) const {
    return detail::encode(block_, buffer, capacity, game_version);
}

Result<void> ActionSet::save(const char* path, std::uint32_t game_version) const {
    if (block_ == nullptr) return Error::make(ErrorCode::InvalidArgument, "empty action set");
    const Result<std::size_t> size = detail::encode(block_, block_->file, kMaxBindingsSize, game_version);
    if (!size) return size.error();
    return save::write(path, block_->file, size.value(), detail::kSaveVersion);
}

Result<LoadReport> ActionSet::load(const char* path, std::uint32_t game_version) {
    if (block_ == nullptr) return Error::make(ErrorCode::InvalidArgument, "empty action set");
    const Result<std::size_t> size = save::read(path, block_->file, kMaxBindingsSize, nullptr);
    if (!size) {
        if (size.error().code == ErrorCode::NotFound) return LoadReport{};
        return size.error();
    }
    return load_from(block_->file, size.value(), game_version);
}

} // namespace nnh
