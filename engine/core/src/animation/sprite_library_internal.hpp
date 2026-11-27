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

/// Hot reload (spec 031, Debug/Development; sprite_reload_table.cpp): a library loaded from an asset path. The game's
/// SpriteLibrary keeps its first block (`identity`); a reload builds `current`; the block before it (`previous`) is kept
/// until the next reload so that animators bound to it can still read its clip names when they rebind.
struct SpriteReloadEntry {
    SpriteBlock* identity = nullptr;
    SpriteBlock* current = nullptr;
    SpriteBlock* previous = nullptr;
    std::uint32_t hash = 0;     ///< reload::path_hash of the asset path
    std::uint16_t serial = 0;   ///< the library's lifetime serial
    std::uint8_t epoch = 0;     ///< reloads applied (animators keep its 2 low bits: a reused address is told apart)
    std::uint16_t extras[6] = {}; ///< LoadSpriteLibraryOptions, for the reloaded blocks
    char name[32] = {};         ///< the load path, for warnings
};
inline constexpr std::uint8_t kSpriteForwarded = 1; ///< Header::reserved[0] of an identity block that was reloaded
/// Reloads applied to any library so far: 0 keeps the animators' check to one load.
extern int g_sprite_reloads;
SpriteReloadEntry* sprite_reload_entry(const SpriteBlock* identity);
SpriteReloadEntry* sprite_reload_entry_of_serial(std::uint16_t serial);
/// The tracked libraries (`count` entries, some empty: identity null).
SpriteReloadEntry* sprite_reload_entries(int& count);
/// load_sprite_library made `identity` from `asset_path` (false: the table is full or has no memory; not tracked).
bool sprite_reload_track(SpriteBlock* identity, const char* asset_path, const char* name,
                         const LoadSpriteLibraryOptions& options);
/// The library is destroyed: its reloaded blocks are freed and its entry forgotten (the identity is the caller's).
void sprite_reload_forget(SpriteBlock* identity);
/// sprite_reload.cpp: registers the kind and tracks the library (called by load_sprite_library).
void sprite_reload_loaded(SpriteBlock* identity, const char* asset_path, const char* name,
                          const LoadSpriteLibraryOptions& options);

/// A view of the library's content: in Debug/Development, a reloaded library's identity block forwards to its current
/// block (one flag test for every other library).
inline SpriteView sprite_view(const SpriteBlock* block) {
#if !defined(NNH_FLAVOR_RELEASE)
    if (block != nullptr && block->header.reserved[0] == kSpriteForwarded) {
        if (const SpriteReloadEntry* e = sprite_reload_entry(block)) block = e->current;
    }
#endif
    return SpriteView{const_cast<SpriteBlock*>(block)};
}

/// Validates a `.nnhsprite` file and copies it into a new block (not adopted: no serial) with `extras` (the six
/// LoadSpriteLibraryOptions values). Errors as load_sprite_library's (logged with `name`).
Result<SpriteBlock*> sprite_block_from_file(const void* data, std::size_t size, const int (&extras)[6],
                                            const char* name);

/// The memory tag of every library block ("animation").
MemTag& animation_memory_tag();

/// A new lifetime serial (never 0) whose Debug/Development live slot is free, marked live.
std::uint16_t sprite_take_serial();
/// Marks a serial dead (the library was destroyed).
void sprite_release_serial(std::uint16_t serial);
#if !defined(NNH_FLAVOR_RELEASE)
inline constexpr int kSpriteLiveSlots = 1024;
/// Debug/Development: the serial of the live library in slot `serial & 1023`, 0 = free (sprite_library.cpp).
extern std::uint16_t g_sprite_live[kSpriteLiveSlots];
#endif

/// Debug/Development: false when the library of `serial` was destroyed. Release: always true. Inline: every animator
/// command and update makes this check.
inline bool sprite_serial_live(std::uint16_t serial) {
#if !defined(NNH_FLAVOR_RELEASE)
    return serial != 0 && g_sprite_live[serial & (kSpriteLiveSlots - 1)] == serial;
#else
    (void)serial;
    return true;
#endif
}

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
