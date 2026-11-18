#pragma once
// The audio module for the frame loop and the other audio files. Internal.

#include <cstddef>
#include <cstdint>

#include "assets/asset_path.hpp"
#include "audio/mix.hpp"
#include "audio/voice_table.hpp"
#include "nnh/app.hpp"
#include "nnh/audio.hpp"
#include "nnh/memory.hpp"
#include "nnh/platform/audio.hpp"
#include "nnh/time.hpp"

namespace nnh {

/// Reaches into nnh::Sound for the engine (friend of Sound).
struct SoundAccess {
    static int slot(const Sound& s) { return s.slot_; }
    static std::uint32_t generation(const Sound& s) { return s.generation_; }
    static Sound make(int slot, std::uint32_t generation) {
        Sound s;
        s.slot_ = slot;
        s.generation_ = generation;
        return s;
    }
};

namespace detail {

/// Decoded sounds are limited to this many bytes (longer audio is music, streamed).
inline constexpr std::size_t kMaxSoundBytes = 4u * 1024 * 1024;
/// The 3DS DSP has 24 channels; 2 are kept for music.
inline constexpr int kMaxVoices = 22;

struct SoundSlot {
    bool active = false;
    std::uint32_t generation = 0;
    platform::SampleHandle sample = 0; ///< 0 when audio is unavailable
    std::uint32_t frames = 0;
    int channels = 0;
    std::uint32_t rate = 0;
    std::size_t bytes = 0;
    bool drop_logged = false;
};

/// Music's part of the frame loop (music.cpp). Registered the first time a game uses music::, so a game
/// without music doesn't link the decoders (stb_vorbis is ~60 KB on the 3DS).
struct MusicHooks {
    void (*frame)(Duration delta);
    void (*shutdown)();
    void (*mix_changed)(); ///< group volume or pause changed: re-send the streams' gains and pause
};

struct AudioState {
    bool ready = false;
    bool available = false; ///< the platform has audio output
    VoiceTable voices;
    VoiceState* voice_storage = nullptr;
    SoundSlot* sounds = nullptr;
    int max_sounds = 0;
    Mixer mixer;
    const MusicHooks* music = nullptr; ///< set by music.cpp on first use
};

AudioState& audio_state();
MemTag& audio_memory_tag();

/// nnh::init(): tables from `config`, then the platform's output (its absence is logged, not an error).
/// @return false only if the tables can't be allocated.
bool audio_init(const AudioConfig& config);
void audio_shutdown();

/// Once per frame, before events are dispatched: ends finished voices and publishes SoundFinished.
void audio_frame(Duration delta);

/// The asset of a sound or track `path` given without extension: path.wav, else path.ogg (written to
/// `out`). @return true for Ogg. Errors (logged): InvalidArgument, NotFound.
Result<bool> find_audio_asset(const char* path, char (&out)[kMaxAssetPath + 1]);


/// The live sound behind a handle, or nullptr.
SoundSlot* sound_get(int slot, std::uint32_t generation);

/// Calls `fn(index)` for every active voice.
template <typename Fn>
void for_each_voice(Fn&& fn) {
    AudioState& a = audio_state();
    for (int i = 0; i < a.voices.count(); ++i) {
        if (a.voices.at(i).active) fn(i);
    }
}

/// Stops a voice on the platform (if any) and frees it, without an event.
void end_voice(int index);

/// Stops (without an event) every voice playing a sound slot.
void stop_voices_of(int sound_slot);

/// The platform sample of a Sound (0 if none); for tests.
platform::SampleHandle sound_sample(const Sound& sound);

} // namespace detail
} // namespace nnh
