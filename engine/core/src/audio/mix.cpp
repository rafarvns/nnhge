#include "audio/mix.hpp"

namespace nnh::detail {

namespace {

float clamp(float v, float lo, float hi, float if_nan) {
    if (!(v == v)) return if_nan;
    return v < lo ? lo : (v > hi ? hi : v);
}

} // namespace

float clamp_volume(float v) { return clamp(v, 0.0f, 1.0f, 0.0f); }
float clamp_pan(float p) { return clamp(p, -1.0f, 1.0f, 0.0f); }
float clamp_pitch(float p) { return clamp(p, 0.25f, 4.0f, 1.0f); }

platform::VoiceParams Mixer::params(float volume, float pan, float pitch, std::uint32_t sample_rate,
                                    AudioGroup group) const {
    const float gain = clamp_volume(volume) * group_volume(group) * master_;
    const float p = clamp_pan(pan);
    platform::VoiceParams out;
    out.gain_left = gain * (p > 0.0f ? 1.0f - p : 1.0f);
    out.gain_right = gain * (p < 0.0f ? 1.0f + p : 1.0f);
    out.rate = static_cast<float>(sample_rate) * clamp_pitch(pitch);
    return out;
}

} // namespace nnh::detail
