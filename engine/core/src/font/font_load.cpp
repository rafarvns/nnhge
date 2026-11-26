// load_font (nnh/font.hpp; spec 029 FR-5, D49): `<path>.nnhfont` read whole, validated, its tables copied into one block
// (tag `font`), each page decoded into a temporary buffer and made a texture, then its fallback fonts (one level; loaded
// later for an entry first loaded as a fallback; never in a loop). A path loaded twice shares one entry (its hash and its
// path both match). Any failure releases what was made. Also the release of the last owner.

#include <cstdio>
#include <cstring>

#include "assets/asset_path.hpp"
#include "core/shutdown_hooks.hpp"
#include "font/font_internal.hpp"
#include "font/font_table.hpp"
#include "font/page_codec.hpp"
#include "gfx/gfx_state.hpp"
#include "gfx/texture_alpha.hpp"
#include "memory/memory_internal.hpp"
#include "nnh/assets.hpp"
#include "nnh/detail/name_hash.hpp"
#include "nnh/log.hpp"
#include "nnh/memory.hpp"
#include "nnh/profile.hpp"
#include "text/font_resolve.hpp"
#include "text/font_texture.hpp"
#include "text/text_missing.hpp"

namespace nnh {

NNH_LOG_CHANNEL(font);

MemTag& font_block_tag() { return detail::font_memory_tag(); }

namespace {

using detail::FontBlock;
namespace ff = detail::font;

Error refuse(ErrorCode code, const char* path, const char* what) {
    NNH_LOG_WARN(font, "load_font(\"%s\"): %s", path, what);
    return Error::make(code, "font \"%s\": %s", path, what);
}

/// Frees a block and the pages made so far.
void free_block(FontBlock* block, MemTag& tag) {
    if (block == nullptr) return;
    for (int i = 0; i < block->header.page_count; ++i) {
        if (block->page_slot[i] >= 0) detail::texture_release_page(block->page_slot[i], block->page_generation[i], tag);
    }
    mem::free(block);
}

/// Decodes page `i` of `file` into a temporary buffer and makes its texture. @return nullptr or the error's text.
const char* make_page(FontBlock& block, int i, const std::uint8_t* file, MemTag& tag, ErrorCode& code) {
    const ff::PageRecord& p = block.pages[i];
    const bool rgba = p.format == ff::kPageRgba8;
    const std::size_t pixels = std::size_t{p.width} * p.height;
    const std::size_t decoded = ff::page_decoded_bytes(p);
    const std::size_t room = rgba ? decoded : pixels; // A4: decoded into the upper half, unpacked in place
    auto* buffer = static_cast<std::uint8_t*>(mem::alloc(MemRegion::Heap, tag, room, 4));
    code = ErrorCode::OutOfMemory;
    if (buffer == nullptr) return "no memory to decode a page";
    std::uint8_t* packed = buffer + (room - decoded);
    if (p.codec == ff::kCodecRaw) std::memcpy(packed, file + p.offset, decoded);
    else ff::packbits_decode(file + p.offset, p.bytes, packed, decoded); // checked by the validator
    if (!rgba) ff::a4_unpack(packed, pixels, buffer);
    std::uint32_t generation = 0;
    const int slot = detail::texture_from_page(buffer, p.width, p.height, rgba, tag, generation);
    mem::free(buffer);
    if (slot < 0) {
        code = slot == -2 ? ErrorCode::LimitReached : ErrorCode::OutOfMemory;
        return slot == -2 ? "too many textures (AppConfig::gfx.max_textures)" : "no memory for a page texture";
    }
    block.page_slot[i] = static_cast<std::int16_t>(slot);
    block.page_generation[i] = generation;
    block.page_bytes[i] = static_cast<std::uint32_t>(detail::gfx_state().textures.get(slot, generation)->bytes);
    return nullptr;
}

Result<int> load_entry(const char* path, MemTag& tag, bool with_fallbacks);

/// An entry's pages are still gfx textures: false for an entry of an earlier engine run (Release skips the shutdown hook,
/// so its table outlives nnh::shutdown; load_entry then frees its block and forgets it).
bool alive(const FontBlock& block) {
    return block.header.page_count == 0 ||
           detail::gfx_state().textures.get(block.page_slot[0], block.page_generation[0]) != nullptr;
}

/// Pages gfx destroyed at nnh::shutdown were taken off the `textures` tag; they were accounted under the font's: move
/// them back so both tags end at zero.
void settle_dead_pages(const FontBlock& block, MemTag& tag) {
    for (int i = 0; i < block.header.page_count; ++i) {
        detail::mem_account(MemRegion::Linear, tag, block.page_bytes[i], false);
        detail::mem_account(MemRegion::Linear, NNH_MEM_TAG_REF(textures), block.page_bytes[i], true);
    }
}

/// The tag a table entry was loaded under.
MemTag& entry_tag(int slot) {
    MemTag* tag = detail::font_table_extra(slot).tag;
    return tag != nullptr ? *tag : detail::font_memory_tag();
}

/// nnh::shutdown (Debug/Development): gfx already destroyed the pages; the blocks are freed and the table emptied.
void fonts_shutdown() {
    for (int slot = 0; slot < detail::kFontSlots; ++slot) {
        if (FontBlock* block = detail::font_table_block(detail::font_table_id(slot))) {
            settle_dead_pages(*block, entry_tag(slot));
            mem::free(block);
        }
    }
    detail::font_table_reset();
    detail::g_font_resolve = nullptr;
    detail::text_missing_reset();
}

/// `from` reaches `target` through fallback references (the table stays without loops, so this ends).
bool reaches(int from, int target, int depth = 0) {
    if (from == target) return true;
    if (depth >= detail::kFontSlots) return false;
    const detail::FontEntryExtra& e = detail::font_table_extra(from);
    for (int i = 0; i < e.fallback_count; ++i)
        if (e.fallbacks[i] >= 0 && reaches(e.fallbacks[i], target, depth + 1)) return true;
    return false;
}

/// Drops the fallbacks the entry in `slot` holds (a failed load_fallbacks).
void drop_fallbacks(int slot) {
    detail::FontEntryExtra& extra = detail::font_table_extra(slot);
    const int n = extra.fallback_count;
    int held[ff::kMaxFallbacks];
    for (int i = 0; i < n; ++i) held[i] = extra.fallbacks[i];
    extra.fallback_count = 0;
    for (int i = 0; i < ff::kMaxFallbacks; ++i) extra.fallbacks[i] = -1;
    for (int i = 0; i < n; ++i) detail::font_release(detail::font_table_id(held[i]));
}

/// Loads the fallbacks of the entry in `slot` (one level): one naming the font itself is refused (review N2); one that
/// already falls back to it, directly or not, is skipped, so no two entries keep each other alive. @return the error.
Result<int> load_fallbacks(int slot, std::uint32_t own_hash, const char* path, const FontBlock& block, MemTag& tag) {
    detail::FontEntryExtra& extra = detail::font_table_extra(slot);
    for (int i = 0; i < block.header.fallback_count; ++i) {
        const std::uint32_t hash = detail::fnv1a32(block.fallbacks[i]);
        if (hash == own_hash) return refuse(ErrorCode::Corrupted, path, "it names itself as a fallback");
        const int existing = detail::font_table_find(hash);
        if (existing >= 0 && reaches(existing, slot)) {
            NNH_LOG_INFO(font, "%s: fallback %s skipped: it falls back to this font", path, block.fallbacks[i]);
            continue;
        }
        auto loaded = load_entry(block.fallbacks[i], tag, false);
        if (!loaded) return loaded.error();
        extra.fallbacks[extra.fallback_count++] = loaded.value();
    }
    extra.fallbacks_loaded = true;
    return slot;
}

/// The table slot of `path` with one more reference: found, or loaded.
Result<int> load_entry(const char* path, MemTag& tag, bool with_fallbacks) {
    const std::uint32_t hash = detail::fnv1a32(path);
    int found = detail::font_table_find(hash);
    if (found >= 0 && !alive(*detail::font_table_block(detail::font_table_id(found)))) {
        FontBlock* stale = detail::font_table_block(detail::font_table_id(found));
        settle_dead_pages(*stale, entry_tag(found));
        mem::free(stale); // its memory, not leaked across runs
        detail::font_table_forget(found);
        found = -1;
    }
    if (found >= 0) {
        // the table matches by hash: a different path with the same hash is refused, not shared (review N2)
        if (std::strncmp(detail::font_table_extra(found).path, path, ff::kPathBytes - 1) != 0) {
            return refuse(ErrorCode::InvalidArgument, path, "its path has the hash of another loaded font's: rename one");
        }
        detail::font_table_retain(found);
        const FontBlock& block = *detail::font_table_block(detail::font_table_id(found));
        // first loaded as another font's fallback (without its own): load_font() of it now loads them (review S2)
        if (with_fallbacks && !detail::font_table_extra(found).fallbacks_loaded && block.header.fallback_count > 0) {
            auto done = load_fallbacks(found, hash, path, block, tag);
            if (!done) {
                drop_fallbacks(found);
                detail::font_release(detail::font_table_id(found));
                return done.error();
            }
        }
        return found;
    }
    char asset[detail::kMaxAssetPath + 1];
    if (std::snprintf(asset, sizeof asset, "%s.nnhfont", path) >= static_cast<int>(sizeof asset)) {
        return refuse(ErrorCode::InvalidArgument, path, "path too long");
    }
    auto file = assets::load(asset);
    if (!file) return file.error();
    const auto* data = static_cast<const std::uint8_t*>(file.value().data());
    if (const char* why = ff::font_validate(data, file.value().size())) return refuse(ErrorCode::Corrupted, path, why);
    const ff::Header h = ff::read_header(data);
    const std::size_t bytes = detail::font_block_bytes(h);
    void* memory = mem::alloc(MemRegion::Heap, tag, bytes, 8);
    if (memory == nullptr) return refuse(ErrorCode::OutOfMemory, path, "no memory for the font");
    auto* block = const_cast<FontBlock*>(detail::font_block_init(memory, bytes, data, h.off_page_data));
    for (int i = 0; i < ff::kMaxPages; ++i) block->page_slot[i] = -1;
    const int slot = detail::font_table_add(hash, block);
    if (slot < 0) {
        mem::free(memory);
        return refuse(ErrorCode::LimitReached, path, "32 fonts are loaded already");
    }
    block->self_slot = static_cast<std::uint8_t>(slot);
    detail::FontEntryExtra& extra = detail::font_table_extra(slot);
    extra.tag = &tag;
    std::strncpy(extra.path, path, sizeof extra.path - 1);
    block->name = extra.path;
    for (int i = 0; i < h.page_count; ++i) {
        ErrorCode code = ErrorCode::None;
        if (const char* why = make_page(*block, i, data, tag, code)) {
            detail::font_release(detail::font_table_id(slot));
            return refuse(code, path, why);
        }
    }
    if (!with_fallbacks && h.fallback_count > 0) {
        NNH_LOG_INFO(font, "%s: its own fallback fonts are ignored (one level)", path);
    }
    if (with_fallbacks) {
        auto done = load_fallbacks(slot, hash, path, *block, tag);
        if (!done) {
            detail::font_release(detail::font_table_id(slot));
            return done.error();
        }
    }
    return slot;
}

} // namespace

void detail::font_release(std::uint32_t id) {
    const int slot = font_table_slot(id);
    if (slot < 0) return;
    const FontEntryExtra extra = font_table_extra(slot); // copied: the entry is emptied below
    FontBlock* block = font_table_release(slot);
    if (block == nullptr) return;
    for (int i = 0; i < extra.fallback_count; ++i) {
        if (extra.fallbacks[i] >= 0) font_release(font_table_id(extra.fallbacks[i]));
    }
    free_block(block, extra.tag != nullptr ? *extra.tag : font_memory_tag());
}

bool detail::font_resolve_face(std::uint32_t id, TextFace& face) {
    const int slot = font_table_slot(id);
    if (slot < 0 || !alive(*font_table_block(id))) return false;
    face.fonts[0] = font_table_block(id);
    face.count = 1;
    const FontEntryExtra& extra = font_table_extra(slot);
    for (int i = 0; i < extra.fallback_count && face.count < kFaceFonts; ++i) {
        if (FontBlock* f = font_table_block(font_table_id(extra.fallbacks[i]))) face.fonts[face.count++] = f;
    }
    return true;
}

std::size_t detail::font_bytes(int slot) {
    FontBlock* f = font_table_block(font_table_id(slot));
    if (f == nullptr) return 0;
    std::size_t bytes = font_block_bytes(f->header);
    for (int i = 0; i < f->header.page_count; ++i) {
        const ff::PageRecord& p = f->pages[i];
        bytes += std::size_t{p.width} * p.height * (p.format == ff::kPageRgba8 ? 4u : 1u);
    }
    return bytes;
}

Result<Font> load_font(const char* path, MemTag* tag) {
    NNH_PROFILE_SCOPE("load_font");
    if (path == nullptr || path[0] == 0) return Error::make(ErrorCode::InvalidArgument, "load_font(\"\")");
    if (!detail::gfx_state().ready) return Error::make(ErrorCode::Unsupported, "load_font(\"%s\") before nnh::init()", path);
    auto slot = load_entry(path, tag != nullptr ? *tag : detail::font_memory_tag(), true);
    if (!slot) return slot.error();
    detail::g_font_resolve = &detail::font_resolve_face;
    detail::add_shutdown_hook(&fonts_shutdown);
    return FontAccess::make(detail::font_table_id(slot.value()));
}

} // namespace nnh
