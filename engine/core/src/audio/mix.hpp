#pragma once
// Mixer rules (spec 008 FR-4): group and master volumes, group pause, and the per-speaker gains and
// rate a voice plays with. Pan is a balance: the centre plays at full volume on both speakers, panning
// right lowers only the left one. Pure; unit tested in tests/audio/test_mix.cpp.

#include <cstdint>

#include "nnh/audio.hpp"
#include "nnh/platform/audio.hpp"

namespace nnh::detail {

float clamp_volume(float v); ///< 0..1 (NaN -> 0)
float clamp_pan(float p);    ///< -1..1 (NaN -> 0)
float clamp_pitch(float p);  ///< 0.25..4 (NaN -> 1)

class Mixer {
public:
    void set_group_volume(AudioGroup group, float volume) { group_volume_[index(group)] = clamp_volume(volume); }
    float group_volume(AudioGroup group) const { return group_volume_[index(group)]; }
    void set_master(float volume) { master_ = clamp_volume(volume); }
    float master() const { return master_; }
    void set_group_paused(AudioGroup group, bool paused) { group_paused_[index(group)] = paused; }
    bool group_paused(AudioGroup group) const { return group_paused_[index(group)]; }

    /// Whether a voice is silent-paused: its own pause or its group's.
    bool paused(bool voice_paused, AudioGroup group) const { return voice_paused || group_paused(group); }

    /// What the platform plays for a voice (inputs are clamped).
    platform::VoiceParams params(float volume, float pan, float pitch, std::uint32_t sample_rate, AudioGroup group) const;

private:
    static int index(AudioGroup g) { return static_cast<int>(g) % kAudioGroupCount; }
    float group_volume_[kAudioGroupCount] = {1.0f, 1.0f, 1.0f};
    bool group_paused_[kAudioGroupCount] = {};
    float master_ = 1.0f;
};

} // namespace nnh::detail
