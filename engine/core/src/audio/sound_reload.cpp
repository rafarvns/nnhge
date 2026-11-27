// The sound kind of hot reload (spec 031 FR-7, FR-9, Q7): each sound slot loaded from an asset path gets the path's hash
// in a Debug/Development side table; a reloaded `*.wav`/`*.ogg` is decoded for every slot loaded from it at prepare
// (new samples), and at commit the voices playing it stop — each with its SoundFinished, delivered in the same frame —
// and the slot's samples are swapped behind the game's Sound handles (the next play uses the new samples). The old
// platform samples are freed at the next batch or at shutdown (the 3DS stops a channel asynchronously).
// Tests: tests/audio/test_sound_reload.cpp.

#include <cstring>

#include "audio/audio_internal.hpp"
#include "core/shutdown_hooks.hpp"
#include "memory/memory_internal.hpp"
#include "nnh/events.hpp"
#include "nnh/memory.hpp"
#include "reload/reload_kind.hpp"
#include "reload/reload_memory.hpp"
#include "reload/reload_overlay.hpp"

namespace nnh::detail {
namespace {

using reload::Prepared;
using reload::ReloadItem;
using remote::ReloadReason;
using remote::ReloadStage;

struct SlotTag {
    std::uint32_t hash = 0;
    std::uint32_t generation = 0;
};
SlotTag* g_tags = nullptr;
int g_tag_count = 0;

constexpr int kMaxRetired = 32;
platform::SampleHandle g_retired[kMaxRetired];
int g_retired_count = 0;

struct NewSound {
    int slot = 0;
    SoundSlot data;
};
struct PreparedSounds {
    int count = 0;
    NewSound items[1];
};

void free_retired() {
    for (int i = 0; i < g_retired_count; ++i) platform::sample_destroy(g_retired[i]);
    g_retired_count = 0;
}

bool claims(const ReloadItem& item) {
    const std::size_t n = std::strlen(item.path);
    return n > 4 && (std::strcmp(item.path + n - 4, ".wav") == 0 || std::strcmp(item.path + n - 4, ".ogg") == 0);
}

bool live(int i) { return sound_get(i, g_tags[i].generation) != nullptr; }

void destroy_prepared(PreparedSounds* p) {
    for (int i = 0; i < p->count; ++i) {
        if (p->items[i].data.sample != 0) platform::sample_destroy(p->items[i].data.sample);
        if (p->items[i].data.bytes > 0) mem_account(MemRegion::Linear, audio_memory_tag(), p->items[i].data.bytes, false);
    }
    mem::free(p);
}

Prepared prepare(const ReloadItem& item) {
    int count = 0;
    for (int i = 0; i < g_tag_count && item.op == remote::ReloadOp::Put; ++i) {
        if (g_tags[i].hash == item.hash && live(i)) ++count;
    }
    if (count == 0) return reload::prepared_ok();
    free_retired(); // the previous batch's samples: the platform has long stopped them
    auto file = assets::load(item.path);
    if (!file) return reload::prepared_refused(ReloadStage::Memory, ReloadReason::NoMemory, item.size);
    const std::size_t bytes = sizeof(PreparedSounds) + sizeof(NewSound) * static_cast<std::size_t>(count - 1);
    auto* p = static_cast<PreparedSounds*>(mem::alloc(MemRegion::Heap, reload::reload_memory_tag(), bytes));
    if (p == nullptr) return reload::prepared_refused(ReloadStage::Memory, ReloadReason::NoMemory, bytes);
    p->count = 0;
    const std::size_t n = std::strlen(item.path);
    const bool ogg = std::strcmp(item.path + n - 4, ".ogg") == 0;
    for (int i = 0; i < g_tag_count; ++i) {
        if (g_tags[i].hash != item.hash || !live(i)) continue;
        NewSound& s = p->items[p->count];
        s.slot = i;
        auto decoded = decode_sound(item.path, file.value(), ogg, s.data);
        if (!decoded) {
            destroy_prepared(p);
            const bool memory = decoded.error().code == ErrorCode::OutOfMemory;
            return reload::prepared_refused(memory ? ReloadStage::Memory : ReloadStage::Validate,
                                            memory ? ReloadReason::NoMemory : ReloadReason::Corrupted, item.size);
        }
        ++p->count;
    }
    return reload::prepared_ok(p);
}

void commit(const ReloadItem&, void* state) {
    auto* p = static_cast<PreparedSounds*>(state);
    if (p == nullptr) return;
    AudioState& a = audio_state();
    for (int k = 0; k < p->count; ++k) {
        const NewSound& n = p->items[k];
        for_each_voice([&](int i) { // Q7: stopped at the reload point, each with its SoundFinished
            VoiceState& v = a.voices.at(i);
            if (v.sound_slot != n.slot) return;
            const Voice handle{static_cast<std::uint16_t>(i), v.generation};
            end_voice(i);
            events::publish(SoundFinished{handle});
        });
        SoundSlot& s = a.sounds[n.slot];
        if (s.sample != 0 && g_retired_count < kMaxRetired) g_retired[g_retired_count++] = s.sample;
        else if (s.sample != 0) platform::sample_destroy(s.sample);
        if (s.bytes > 0) mem_account(MemRegion::Linear, audio_memory_tag(), s.bytes, false);
        const std::uint32_t generation = s.generation;
        s = n.data;
        s.active = true;
        s.generation = generation; // the game's Sound handles stay valid
    }
    mem::free(p);
}

void abandon(const ReloadItem&, void* state) {
    if (auto* p = static_cast<PreparedSounds*>(state)) destroy_prepared(p);
}

reload::ReloadKind g_kind{"sound", claims, prepare, commit, abandon};

void shutdown_hook() {
    free_retired();
    mem::free(g_tags);
    g_tags = nullptr;
    g_tag_count = 0;
}

} // namespace

void sound_reload_loaded(int slot, std::uint32_t generation, const char* asset_path) {
    if (g_tags == nullptr) {
        const int capacity = audio_state().max_sounds;
        g_tags = static_cast<SlotTag*>(mem::alloc(MemRegion::Heap, reload::reload_memory_tag(),
                                                  sizeof(SlotTag) * static_cast<std::size_t>(capacity)));
        if (g_tags == nullptr) return;
        g_tag_count = capacity;
        for (int i = 0; i < capacity; ++i) g_tags[i] = SlotTag{};
        require_shutdown_hook(shutdown_hook);
    }
    reload::register_kind(g_kind);
    if (slot >= 0 && slot < g_tag_count) g_tags[slot] = SlotTag{reload::path_hash(asset_path), generation};
}

} // namespace nnh::detail
