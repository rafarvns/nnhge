// Text sources (spec 028 FR-24): copies in the UI's text bytes (NUL-terminated, compacted when released), borrowed
// pointers, keys resolved by the game's resolver and cached until text_changed() (the localization spec's hook); and
// the function slots holding callbacks, row fills and formatters.

#include <cstring>

#include "ui/ui_block.hpp"

namespace nnh::detail {
namespace {

constexpr std::uint8_t kResolvedBit = 0x80; ///< in text_kinds: a choice option's key is resolved

/// Shifts every copied-text offset above `at` down by `n`.
void shift_copies(UiBlock& b, std::uint32_t at, std::uint32_t n) {
    for (int s = 0; s < b.capacity; ++s) {
        if (b.hot[s].kind == kUiFreeKind) continue;
        UiCold& c = b.cold[s];
        if (c.text_kind == kUiTextCopy && c.text.word > at) c.text.word -= n;
    }
    for (int i = 0; i < b.texts_used; ++i) {
        if ((b.text_kinds[i] & 0x7F) == kUiTextCopy && b.texts[i].word > at) b.texts[i].word -= n;
    }
}

} // namespace

bool store_text(UiBlock& b, const UiText& t, std::uint8_t& kind, UiTextRef& ref) {
    ref = UiTextRef{};
    switch (t.kind()) {
    case UiText::Kind::None: kind = kUiTextNone; return true;
    case UiText::Kind::Borrow:
        kind = kUiTextBorrow;
        ref.ptr = t.text();
        return true;
    case UiText::Kind::Key:
        kind = kUiTextKey;
        ref.word = t.key_value();
        return true;
    case UiText::Kind::Copy: break;
    }
    const std::size_t length = std::strlen(t.text());
    if (static_cast<std::size_t>(b.text_used) + length + 1 > static_cast<std::size_t>(b.text_capacity)) {
        kind = kUiTextNone;
        return false;
    }
    std::memcpy(b.text + b.text_used, t.text(), length + 1);
    kind = kUiTextCopy;
    ref.word = static_cast<std::uint32_t>(b.text_used);
    b.text_used += static_cast<int>(length + 1);
    return true;
}

void release_text(UiBlock& b, std::uint8_t kind, const UiTextRef& ref) {
    if ((kind & 0x7F) != kUiTextCopy) return;
    const std::uint32_t at = ref.word;
    const auto n = static_cast<std::uint32_t>(std::strlen(b.text + at) + 1);
    const std::uint32_t tail = static_cast<std::uint32_t>(b.text_used) - at - n;
    std::memmove(b.text + at, b.text + at + n, tail);
    b.text_used -= static_cast<int>(n);
    shift_copies(b, at, n);
}

const char* text_of_ref(UiBlock& b, std::uint8_t kind, UiTextRef& ref, bool& resolved) {
    switch (kind & 0x7F) {
    case kUiTextCopy: return b.text + ref.word;
    case kUiTextBorrow: return ref.ptr != nullptr ? ref.ptr : "";
    case kUiTextKey:
        if (!resolved) {
            ref.ptr = b.resolver ? b.resolver(ref.word) : nullptr;
            resolved = true;
        }
        return ref.ptr != nullptr ? ref.ptr : "";
    default: return "";
    }
}

const char* text_of(UiBlock& b, int s) {
    UiCold& c = b.cold[s];
    bool resolved = (c.cflags & kUiResolved) != 0;
    const char* t = text_of_ref(b, c.text_kind, c.text, resolved);
    if (resolved) c.cflags |= kUiResolved;
    return t;
}

/// A choice option's text (extra text source `i`).
const char* option_text(UiBlock& b, int i) {
    bool resolved = (b.text_kinds[i] & kResolvedBit) != 0;
    const char* t = text_of_ref(b, b.text_kinds[i], b.texts[i], resolved);
    if (resolved) b.text_kinds[i] |= kResolvedBit;
    return t;
}

int fn_store(UiBlock& b, const void* fn) {
    for (int i = 0; i < b.fns_capacity; ++i) {
        if (b.fn_used[i] != 0) continue;
        std::memcpy(b.fns[i].bytes, fn, sizeof(UiFnSlot));
        b.fn_used[i] = 1;
        ++b.fns_used;
        return i;
    }
    return -1;
}

void fn_release(UiBlock& b, std::uint8_t slot) {
    if (slot == kUiNoSlot || slot >= b.fns_capacity || b.fn_used[slot] == 0) return;
    b.fn_used[slot] = 0;
    --b.fns_used;
}

} // namespace nnh::detail

namespace nnh {

void Ui::set_text(UiWidget w, const UiText& t) {
    detail::UiBlock* b = block_;
    if (b == nullptr) return;
    const int s = detail::slot_of(*b, w);
    if (s < 0) {
        NNH_UI_MISUSE(*b, "Ui::set_text: a stale or null widget");
        return;
    }
    detail::UiCold& c = b->cold[s];
    // keep the new text before the old copy is released (the new one may be the old one's bytes)
    const std::uint8_t old_kind = c.text_kind;
    const detail::UiTextRef old = c.text;
    std::uint8_t kind = 0;
    detail::UiTextRef ref;
    if (!detail::store_text(*b, t, kind, ref)) {
        NNH_UI_MISUSE(*b, "Ui::set_text: the text bytes are full (UiDesc::text_bytes)");
        return;
    }
    c.text_kind = kind;
    c.text = ref;
    c.cflags &= static_cast<std::uint8_t>(~detail::kUiResolved);
    detail::release_text(*b, old_kind, old); // shifts the new copy's offset if it lies above
    if (detail::kind_of(*b, s) == UiKind::Label) {
        c.data.label.visible = c.data.label.rate != 0 ? 0 : 0xFFFF;
        c.data.label.accumulator = 0;
    }
    detail::mark_dirty(*b, s);
}

void Ui::mark_text_changed(UiWidget w) {
    detail::UiBlock* b = block_;
    if (b == nullptr) return;
    const int s = detail::slot_of(*b, w);
    if (s < 0) {
        NNH_UI_MISUSE(*b, "Ui::mark_text_changed: a stale or null widget");
        return;
    }
    b->cold[s].cflags &= static_cast<std::uint8_t>(~detail::kUiResolved);
    detail::mark_dirty(*b, s);
}

void Ui::set_resolver(UiResolver r) {
    if (block_ == nullptr) return;
    block_->resolver = r;
    text_changed();
}

void Ui::text_changed() {
    detail::UiBlock* b = block_;
    if (b == nullptr) return;
    for (int s = 0; s < b->capacity; ++s) b->cold[s].cflags &= static_cast<std::uint8_t>(~detail::kUiResolved);
    for (int i = 0; i < b->texts_used; ++i) b->text_kinds[i] &= 0x7F;
    b->dirty[0] = b->dirty[1] = true;
}

} // namespace nnh
