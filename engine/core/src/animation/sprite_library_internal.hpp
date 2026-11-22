#pragma once
// The sprite library's block and the helpers shared by the animation module's files (spec 016). Not used by other
// modules.

#include <cstdint>
#include <cstring>

#include "animation/sprite_format.hpp"
#include "nnh/animation.hpp"
#include "nnh/log.hpp"
#include "nnh/memory.hpp"

NNH_LOG_CHANNEL_EXTERN(animation);

namespace nnh {

namespace detail {

/// A library block: the header, then its sections (sprite_format.hpp). Never constructed: a view of bytes.
struct SpriteBlock {
    spritefmt::Header header;
};

/// Typed access to the sections of a block (header offsets; no bounds checks: the block was validated or built).
struct SpriteView {
    SpriteBlock* block;

    spritefmt::Header& header() const { return block->header; }
    template <typename T>
    T* section(int s) const {
        auto* bytes = reinterpret_cast<std::uint8_t*>(block);
        return reinterpret_cast<T*>(bytes + (s == spritefmt::kFrames ? sizeof(spritefmt::Header)
                                                                    : block->header.offset[s - 1]));
    }
    spritefmt::FrameEntry* frames() const { return section<spritefmt::FrameEntry>(spritefmt::kFrames); }
    spritefmt::ClipEntry* clips() const { return section<spritefmt::ClipEntry>(spritefmt::kClips); }
    spritefmt::ClipFrameEntry* clip_frames() const {
        return section<spritefmt::ClipFrameEntry>(spritefmt::kClipFrames);
    }
    spritefmt::MarkerEntry* markers() const { return section<spritefmt::MarkerEntry>(spritefmt::kMarkers); }
    spritefmt::StateEntry* states() const { return section<spritefmt::StateEntry>(spritefmt::kStates); }
    spritefmt::TransitionEntry* transitions() const {
        return section<spritefmt::TransitionEntry>(spritefmt::kTransitions);
    }
    int count(int s) const { return block->header.count[s]; }
    int cap(int s) const { return block->header.cap[s]; }
};

inline SpriteView sprite_view(const SpriteBlock* block) {
    return SpriteView{const_cast<SpriteBlock*>(block)};
}

/// The memory tag of every library block ("animation").
MemTag& animation_memory_tag();

/// A new lifetime serial (never 0) whose Debug/Development live slot is free, marked live.
std::uint16_t sprite_take_serial();
/// Marks a serial dead (the library was destroyed).
void sprite_release_serial(std::uint16_t serial);
/// Debug/Development: false when the library of `serial` was destroyed. Release: always true.
bool sprite_serial_live(std::uint16_t serial);

/// A name for a clip or a state: 1..31 bytes of valid UTF-8. nullptr when fine, else the reason.
const char* sprite_name_problem(const char* name);
/// Copies a valid name into a zero-padded 32-byte field.
void sprite_store_name(char out[32], const char* name);

/// The clip called `name` (by hash), or an invalid id.
SpriteClipId sprite_find_clip(const SpriteView& v, const char* name);

/// The rule that applies to a request from `from` to `to` (most specific first; none: Now, no keep-time).
spritefmt::TransitionEntry sprite_resolve_rule(const spritefmt::TransitionEntry* rules, int count, std::uint8_t from,
                                                std::uint8_t to);

} // namespace detail

/// Test and loader access to a library's block.
struct SpriteLibraryAccess {
    static detail::SpriteBlock* block(const SpriteLibrary& library) { return library.block_; }
    /// Adopts a block made by mem::alloc with the animation tag (its serial is assigned here).
    static SpriteLibrary adopt(detail::SpriteBlock* block);
};

} // namespace nnh
