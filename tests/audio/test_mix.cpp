// Mixer rules (audio/mix.hpp, spec 008 FR-2, FR-4): gain = voice × group × master; pan is a balance
// (centre at full volume on both sides); pitch scales the rate; group pause; clamping.

#include <cmath>

#include "audio/mix.hpp"
#include "test.hpp"

using nnh::AudioGroup;
using nnh::detail::Mixer;

namespace {

bool near(float a, float b) { return std::fabs(a - b) < 1e-5f; }

} // namespace

NNH_TEST(mix_centre_plays_at_the_voice_volume_on_both_sides) {
    Mixer m;
    const auto p = m.params(0.5f, 0.0f, 1.0f, 32000, AudioGroup::Effects);
    CHECK(near(p.gain_left, 0.5f) && near(p.gain_right, 0.5f));
    CHECK(near(p.rate, 32000.0f));
}

NNH_TEST(mix_pan_is_a_balance) {
    Mixer m;
    const auto left = m.params(1.0f, -1.0f, 1.0f, 22050, AudioGroup::Effects);
    CHECK(near(left.gain_left, 1.0f) && near(left.gain_right, 0.0f));
    const auto right_half = m.params(1.0f, 0.5f, 1.0f, 22050, AudioGroup::Effects);
    CHECK(near(right_half.gain_left, 0.5f) && near(right_half.gain_right, 1.0f));
}

NNH_TEST(mix_multiplies_voice_group_and_master) {
    Mixer m;
    m.set_group_volume(AudioGroup::Music, 0.5f);
    m.set_master(0.5f);
    const auto music = m.params(0.8f, 0.0f, 1.0f, 32000, AudioGroup::Music);
    CHECK(near(music.gain_left, 0.2f) && near(music.gain_right, 0.2f));
    const auto effect = m.params(0.8f, 0.0f, 1.0f, 32000, AudioGroup::Effects); // other group: only master
    CHECK(near(effect.gain_left, 0.4f));
    CHECK(near(m.group_volume(AudioGroup::Music), 0.5f) && near(m.master(), 0.5f));
}

NNH_TEST(mix_pitch_scales_the_rate) {
    Mixer m;
    CHECK(near(m.params(1, 0, 2.0f, 16000, AudioGroup::Effects).rate, 32000.0f));
    CHECK(near(m.params(1, 0, 0.5f, 16000, AudioGroup::Effects).rate, 8000.0f));
}

NNH_TEST(mix_clamps_out_of_range_values) {
    Mixer m;
    m.set_master(3.0f);
    CHECK(near(m.master(), 1.0f));
    m.set_group_volume(AudioGroup::Dialogue, -1.0f);
    CHECK(near(m.group_volume(AudioGroup::Dialogue), 0.0f));
    const auto p = m.params(2.0f, -7.0f, 100.0f, 10000, AudioGroup::Effects);
    CHECK(near(p.gain_left, 1.0f) && near(p.gain_right, 0.0f) && near(p.rate, 40000.0f));
    CHECK(near(m.params(1, 0, 0.0f, 10000, AudioGroup::Effects).rate, 2500.0f));
    CHECK(near(nnh::detail::clamp_pitch(std::nanf("")), 1.0f)); // NaN: back to normal
    CHECK(near(nnh::detail::clamp_volume(std::nanf("")), 0.0f));
    CHECK(near(nnh::detail::clamp_pan(std::nanf("")), 0.0f));
}

NNH_TEST(mix_group_pause_pauses_its_voices) {
    Mixer m;
    m.set_group_paused(AudioGroup::Effects, true);
    CHECK(m.paused(false, AudioGroup::Effects));
    CHECK(!m.paused(false, AudioGroup::Music));
    CHECK(m.paused(true, AudioGroup::Music)); // a paused voice stays paused
    CHECK(m.group_paused(AudioGroup::Effects));
}
