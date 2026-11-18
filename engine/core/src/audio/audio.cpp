// The audio module (nnh/audio.hpp): lifecycle, the per-frame voice check, and the voice and group API.
// Sounds are loaded in sounds.cpp. Without audio output every rule still runs; voices then end by the
// clock (silent_left) instead of by the platform.

#include "audio/audio_internal.hpp"
#include "log/engine_channels.hpp"
#include "memory/memory_internal.hpp"
#include "nnh/events.hpp"
#include "nnh/log.hpp"
#include "nnh/platform/app.hpp"

namespace nnh {

NNH_MEM_TAG(audio);

namespace detail {
namespace {

AudioState g_audio;

template <typename T>
T* allocate(int count) {
    return static_cast<T*>(mem::alloc(MemRegion::Heap, NNH_MEM_TAG_REF(audio), sizeof(T) * static_cast<std::size_t>(count), alignof(T)));
}

void free_tables() {
    mem::free(g_audio.voice_storage);
    mem::free(g_audio.sounds);
    g_audio.voice_storage = nullptr;
    g_audio.sounds = nullptr;
}

} // namespace

AudioState& audio_state() { return g_audio; }

void end_voice(int index) {
    if (g_audio.available) platform::voice_stop(index);
    g_audio.voices.release(index);
}
MemTag& audio_memory_tag() { return NNH_MEM_TAG_REF(audio); }

bool audio_init(const AudioConfig& config) {
    audio_shutdown();
    const int voices = config.voices < 1 ? 1 : (config.voices > kMaxVoices ? kMaxVoices : config.voices);
    const int sounds = config.max_sounds < 1 ? 1 : config.max_sounds;
    g_audio.voice_storage = allocate<VoiceState>(voices);
    g_audio.sounds = allocate<SoundSlot>(sounds);
    if (g_audio.voice_storage == nullptr || g_audio.sounds == nullptr) {
        NNH_LOG_ERROR(audio, "not enough memory for %d voices and %d sounds (AppConfig::audio)", voices, sounds);
        free_tables();
        return false;
    }
    for (int i = 0; i < voices; ++i) g_audio.voice_storage[i] = VoiceState{};
    for (int i = 0; i < sounds; ++i) g_audio.sounds[i] = SoundSlot{};
    g_audio.voices.reset(g_audio.voice_storage, voices);
    g_audio.max_sounds = sounds;
    g_audio.mixer = Mixer{};
    g_audio.available = platform::audio_init(voices);
    if (!g_audio.available) NNH_LOG_WARN(audio, "no audio output, running silently: %s", platform::last_error());
    if (g_audio.available) mem_account(MemRegion::Linear, NNH_MEM_TAG_REF(audio), platform::kStreamBytes, true);
    g_audio.ready = true;
    return true;
}

void audio_shutdown() {
    if (!g_audio.ready) return;
    for_each_voice(end_voice);
    for (int i = 0; i < g_audio.max_sounds; ++i) {
        SoundSlot& s = g_audio.sounds[i];
        if (!s.active) continue;
        if (s.sample != 0) platform::sample_destroy(s.sample);
        if (s.bytes > 0) mem_account(MemRegion::Linear, NNH_MEM_TAG_REF(audio), s.bytes, false);
        s = SoundSlot{};
    }
    if (g_audio.music != nullptr) g_audio.music->shutdown();
    if (g_audio.available) mem_account(MemRegion::Linear, NNH_MEM_TAG_REF(audio), platform::kStreamBytes, false);
    platform::audio_shutdown();
    free_tables();
    g_audio.voices.reset(nullptr, 0);
    g_audio.ready = false;
    g_audio.available = false;
}

void audio_frame(Duration delta) {
    if (!g_audio.ready) return;
    const double seconds = delta.seconds();
    for_each_voice([seconds](int i) {
        VoiceState& v = g_audio.voices.at(i);
        bool ended = false;
        if (g_audio.available) {
            ended = !platform::voice_playing(i);
        } else if (!v.loop && !g_audio.mixer.paused(v.paused, v.group)) {
            v.silent_left -= seconds;
            ended = v.silent_left <= 0;
        }
        if (!ended) return;
        const Voice handle{static_cast<std::uint16_t>(i), v.generation};
        g_audio.voices.release(i);
        events::publish(SoundFinished{handle});
    });
    if (g_audio.music != nullptr) g_audio.music->frame(delta);
}

void stop_voices_of(int sound_slot) {
    for_each_voice([sound_slot](int i) {
        if (g_audio.voices.at(i).sound_slot == sound_slot) end_voice(i);
    });
}

} // namespace detail


} // namespace nnh
