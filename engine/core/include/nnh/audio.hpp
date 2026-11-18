#pragma once
/// @file nnh/audio.hpp
/// Sound effects and the mixer (spec 008). Sounds are WAV or Ogg Vorbis files in the game's assets,
/// loaded by path; each play() is a voice with its own volume, pan and pitch. See docs/api/audio.md.
///
/// @code
/// auto jump = nnh::load_sound("sfx/jump");          // assets/sfx/jump.wav (or .ogg)
/// if (!jump) { NNH_LOG_ERROR(game, "%s", jump.error().message); return; }
/// nnh::Sound sound = std::move(jump.value());
/// nnh::audio::play(sound, {.volume = 0.8f, .pan = -0.5f});
/// @endcode

#include <cstdint>

#include "nnh/result.hpp"

namespace nnh {

/// Mixer groups: each sound plays in one; the final volume is voice × group × master.
enum class AudioGroup : std::uint8_t { Effects, Music, Dialogue };
inline constexpr int kAudioGroupCount = 3;

/// A sound decoded into memory (16-bit samples). Move-only; freed when destroyed or reset() — voices
/// still playing it stop first. Harmless after nnh::shutdown() (it just becomes invalid).
class Sound {
public:
    Sound() = default;
    ~Sound() { reset(); }
    Sound(Sound&& other) noexcept;
    Sound& operator=(Sound&& other) noexcept;
    Sound(const Sound&) = delete;
    Sound& operator=(const Sound&) = delete;

    /// Stops its voices and frees it now. The Sound becomes empty.
    void reset();
    bool valid() const;
    /// Length in seconds at normal pitch.
    float duration() const;

private:
    friend struct SoundAccess;
    int slot_ = -1;
    std::uint32_t generation_ = 0;
};

/// Loads a sound from the game's assets: `path` without extension, "sfx/jump" → sfx/jump.wav, else
/// sfx/jump.ogg. WAV: PCM 8 or 16-bit; both mono or stereo, 8000–48000 Hz (the build checks this).
/// Load time only (decodes the whole file; use music for long tracks).
/// Errors: NotFound, InvalidArgument (bad path), Unsupported (format), Corrupted, OutOfMemory,
/// LimitReached (AppConfig::audio.max_sounds loaded, or longer than 4 MB decoded), Unsupported before init().
Result<Sound> load_sound(const char* path);

/// One playing instance of a sound. A value: copy it freely; once the voice ends (or was stopped or
/// replaced by another sound) the handle is stale and every call with it does nothing.
struct Voice {
    std::uint16_t index = 0;
    std::uint16_t generation = 0; ///< 0 = no voice (e.g. play() dropped the sound)
    bool operator==(const Voice&) const = default;
};

struct PlayOptions {
    float volume = 1.0f;  ///< 0..1
    float pan = 0.0f;     ///< -1 left .. 0 both .. +1 right (balance: the centre is at full volume)
    float pitch = 1.0f;   ///< 0.25..4; changes speed too
    bool loop = false;    ///< repeats until stop()
    AudioGroup group = AudioGroup::Effects;
    std::uint8_t priority = 128; ///< when every voice is busy, a new sound replaces the lowest-priority
                                 ///< (then oldest) one if that priority is not higher than its own
};

/// Published in nnh::frame() when a voice played to its end (not for stop() or a replaced voice).
struct SoundFinished {
    Voice voice;
};

namespace audio {

/// Plays a sound. Values out of range are clamped. @return the voice, or a {} voice if the sound is
/// invalid or every voice is busy with higher priorities (logged once per sound).
Voice play(const Sound& sound, const PlayOptions& options = {});
void stop(Voice voice);
void set_paused(Voice voice, bool paused);
void set_volume(Voice voice, float volume);
void set_pan(Voice voice, float pan);
void set_pitch(Voice voice, float pitch);
/// True from play() until the voice ends, is stopped or replaced (paused voices are playing).
bool playing(Voice voice);

void set_volume(AudioGroup group, float volume); ///< 0..1, default 1
float volume(AudioGroup group);
void set_master_volume(float volume);           ///< 0..1, default 1
float master_volume();
/// Pauses or resumes every voice of a group (e.g. effects during a pause menu).
void set_paused(AudioGroup group, bool paused);
bool paused(AudioGroup group);

/// False when the system has no audio output (a 3DS without its DSP firmware, a PC without a sound
/// device): everything still works — loads, voices, events — silently. The reason is logged once.
bool available();

} // namespace audio
} // namespace nnh
