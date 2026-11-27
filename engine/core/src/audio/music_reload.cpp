// The music kind of hot reload (spec 031 FR-7, FR-9): each music stream slot opened from an asset path keeps the path's
// hash in a Debug/Development side table; a reloaded `*.wav`/`*.ogg` that a playing track streams is opened and its header
// read at prepare (through the staged batch: a file that isn't a valid track, or whose loop point is past its end, refuses
// the batch), and at commit the track is reopened on the new file at the same sample (music.cpp's music_reload_reopen).
// The check's decoder is a temporary block (an Ogg arena, 256 KB), freed before prepare returns.
// Tests: tests/audio/test_music_reload.cpp.

#include <cstdint>
#include <cstring>
#include <new>

#include "audio/audio_internal.hpp"
#include "audio/music_track.hpp"
#include "nnh/memory.hpp"
#include "nnh/platform/assets.hpp"
#include "reload/reload_hooks.hpp"
#include "reload/reload_kind.hpp"
#include "reload/reload_memory.hpp"
#include "reload/reload_overlay.hpp"

namespace nnh::detail {
namespace {

using reload::Prepared;
using reload::ReloadItem;
using remote::ReloadReason;
using remote::ReloadStage;

std::uint32_t g_hashes[platform::kStreams] = {};

bool claims(const ReloadItem& item) {
    const std::size_t n = std::strlen(item.path);
    return n > 4 && (std::strcmp(item.path + n - 4, ".wav") == 0 || std::strcmp(item.path + n - 4, ".ogg") == 0);
}

std::size_t read_stream(void* context, std::uint64_t offset, void* out, std::size_t n) {
    std::size_t read = 0;
    return platform::asset_read_at(*static_cast<platform::AssetStream*>(context), offset, out, n, read) ? read : 0;
}

/// Opens `path` as the track of slot `slot` would be (its loop options), then closes it. @return the refusal, or ok.
Prepared check(const ReloadItem& item, int slot) {
    const std::size_t n = std::strlen(item.path);
    const bool ogg = std::strcmp(item.path + n - 4, ".ogg") == 0;
    const std::size_t bytes = sizeof(MusicTrack) + (ogg ? OggDecoder::kDefaultArenaBytes : 0);
    void* block = mem::alloc(MemRegion::Heap, reload::reload_memory_tag(), bytes, 16);
    if (block == nullptr) return reload::prepared_refused(ReloadStage::Memory, ReloadReason::NoMemory, bytes);
    auto* track = new (block) MusicTrack();
    void* arena = ogg ? static_cast<std::uint8_t*>(block) + sizeof(MusicTrack) : nullptr;
    platform::AssetStream stream = 0;
    auto size = assets::size(item.path);
    bool ok = size && g_reload_asset_ops != nullptr && g_reload_asset_ops->open(item.path, stream, true);
    if (ok) {
        const ByteSource source{&stream, size.value(), read_stream};
        ok = track->open(source, ogg, arena, OggDecoder::kDefaultArenaBytes, music_reload_loop_option(slot),
                         music_reload_loops(slot))
                 .ok();
    }
    track->~MusicTrack();
    if (stream != 0) platform::asset_close(stream);
    mem::free(block);
    return ok ? reload::prepared_ok() : reload::prepared_refused(ReloadStage::Validate, ReloadReason::Corrupted, item.size);
}

/// The state handed to commit: the slots to reopen, as a bit mask (+1, so that it's never null).
void* as_state(unsigned mask) { return reinterpret_cast<void*>(static_cast<std::uintptr_t>(mask + 1)); }
unsigned from_state(void* state) { return static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(state) - 1); }

Prepared prepare(const ReloadItem& item) {
    if (item.op != remote::ReloadOp::Put) return reload::prepared_ok(); // a removed file: the stream keeps its handle
    unsigned mask = 0;
    for (int i = 0; i < platform::kStreams; ++i) {
        if (g_hashes[i] != item.hash || !music_reload_active(i)) continue;
        const Prepared checked = check(item, i);
        if (!checked.ok) return checked;
        mask |= 1u << i;
    }
    return mask == 0 ? reload::prepared_ok() : reload::prepared_ok(as_state(mask));
}

void commit(const ReloadItem& item, void* state) {
    if (state == nullptr) return;
    const unsigned mask = from_state(state);
    for (int i = 0; i < platform::kStreams; ++i) {
        if ((mask & (1u << i)) != 0) music_reload_reopen(i, item.path);
    }
}

void abandon(const ReloadItem&, void*) {} // nothing was allocated past prepare

reload::ReloadKind g_kind{"music", claims, prepare, commit, abandon};

} // namespace

void music_reload_opened(int slot, const char* asset_path) {
    reload::register_kind(g_kind);
    if (slot >= 0 && slot < platform::kStreams) g_hashes[slot] = reload::path_hash(asset_path);
}

} // namespace nnh::detail
