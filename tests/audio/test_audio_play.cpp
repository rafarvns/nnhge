// Playing sounds (nnh/audio.hpp, spec 008 FR-2, FR-4, FR-5, FR-6) through the fake platform: voices and
// their parameters, stale handles, groups and master, replacing and dropping, SoundFinished.

#include <cmath>
#include <utility>

#include "audio_test_util.hpp"
#include "nnh/audio.hpp"
#include "nnh/events.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;
namespace audio = nnh::audio;
using nnh::AudioGroup;
using nnh::Voice;

namespace {

std::uint8_t g_wav[64 + 400];
constexpr char kIndex[] = "sfx/beep.wav\n";

nnh::Sound start(nnh::AppConfig config = {}) {
    nnh::shutdown();
    fake::reset();
    const std::size_t size = audio_test::make_wav(g_wav, 1, 16, 16000, 100, [](std::uint32_t i) { return static_cast<int>(i); });
    fake::add_asset(".nnh_assets", kIndex, sizeof kIndex - 1);
    fake::add_asset("sfx/beep.wav", g_wav, size);
    CHECK(nnh::init(config));
    auto sound = nnh::load_sound("sfx/beep");
    CHECK(sound.ok());
    return sound ? std::move(sound.value()) : nnh::Sound{};
}

const fake::FakeVoice& voice(Voice v) { return fake::state().voices[v.index]; }
bool near(float a, float b) { return std::fabs(a - b) < 1e-5f; }

int g_finished = 0;
Voice g_last_finished{};

} // namespace

NNH_TEST(play_starts_a_platform_voice) {
    nnh::Sound sound = start();
    const Voice v = audio::play(sound, {.volume = 0.5f, .loop = true});
    CHECK(v.generation != 0 && audio::playing(v));
    const fake::FakeVoice& fv = voice(v);
    CHECK(fv.playing && fv.loop && fv.count == 100 && fv.channels == 1);
    CHECK(near(fv.params.gain_left, 0.5f) && near(fv.params.gain_right, 0.5f) && near(fv.params.rate, 16000.0f));
    nnh::shutdown();
}

NNH_TEST(one_sound_can_play_on_several_voices) {
    nnh::Sound sound = start();
    const Voice a = audio::play(sound);
    const Voice b = audio::play(sound);
    CHECK(a.index != b.index && audio::playing(a) && audio::playing(b));
    nnh::shutdown();
}

NNH_TEST(voice_settings_reach_the_platform) {
    nnh::Sound sound = start();
    const Voice v = audio::play(sound);
    audio::set_volume(v, 0.25f);
    audio::set_pan(v, 1.0f);
    audio::set_pitch(v, 2.0f);
    CHECK(near(voice(v).params.gain_left, 0.0f) && near(voice(v).params.gain_right, 0.25f));
    CHECK(near(voice(v).params.rate, 32000.0f));
    audio::set_paused(v, true);
    CHECK(voice(v).paused && audio::playing(v));
    audio::set_paused(v, false);
    CHECK(!voice(v).paused);
    nnh::shutdown();
}

NNH_TEST(stop_ends_a_voice_without_an_event) {
    nnh::Sound sound = start();
    g_finished = 0;
    auto sub = nnh::events::subscribe<nnh::SoundFinished>([](const nnh::SoundFinished&) { ++g_finished; });
    const Voice v = audio::play(sound);
    audio::stop(v);
    CHECK(!audio::playing(v) && !voice(v).playing);
    CHECK(nnh::frame() && nnh::frame());
    CHECK(g_finished == 0);
    nnh::shutdown();
}

NNH_TEST(a_voice_that_ends_publishes_sound_finished_once) {
    nnh::Sound sound = start();
    g_finished = 0;
    auto sub = nnh::events::subscribe<nnh::SoundFinished>([](const nnh::SoundFinished& e) {
        ++g_finished;
        g_last_finished = e.voice;
    });
    const Voice v = audio::play(sound);
    CHECK(nnh::frame());
    CHECK(g_finished == 0 && audio::playing(v));
    fake::finish_voice(v.index);
    CHECK(nnh::frame());
    CHECK(g_finished == 1 && g_last_finished == v && !audio::playing(v));
    CHECK(nnh::frame());
    CHECK(g_finished == 1);
    nnh::shutdown();
}

NNH_TEST(stale_voices_change_nothing) {
    nnh::Sound sound = start();
    const Voice old = audio::play(sound, {.volume = 1.0f});
    audio::stop(old);
    const Voice current = audio::play(sound, {.volume = 0.5f}); // may reuse the same platform voice
    audio::set_volume(old, 0.1f);
    audio::set_paused(old, true);
    audio::stop(old);
    CHECK(audio::playing(current));
    CHECK(near(voice(current).params.gain_left, 0.5f) && !voice(current).paused);
    CHECK(!audio::playing(Voice{}));
    nnh::shutdown();
}

NNH_TEST(group_and_master_volume_scale_live_voices) {
    nnh::Sound sound = start();
    const Voice effect = audio::play(sound, {.volume = 0.8f});
    const Voice line = audio::play(sound, {.volume = 0.8f, .group = AudioGroup::Dialogue});
    audio::set_volume(AudioGroup::Effects, 0.5f);
    CHECK(near(voice(effect).params.gain_left, 0.4f) && near(voice(line).params.gain_left, 0.8f));
    audio::set_master_volume(0.5f);
    CHECK(near(voice(effect).params.gain_left, 0.2f) && near(voice(line).params.gain_left, 0.4f));
    CHECK(near(audio::volume(AudioGroup::Effects), 0.5f) && near(audio::master_volume(), 0.5f));
    // New voices get the group volume too.
    const Voice later = audio::play(sound);
    CHECK(near(voice(later).params.gain_left, 0.25f));
    nnh::shutdown();
}

NNH_TEST(group_pause_pauses_only_that_group) {
    nnh::Sound sound = start();
    const Voice effect = audio::play(sound);
    const Voice line = audio::play(sound, {.group = AudioGroup::Dialogue});
    audio::set_paused(AudioGroup::Effects, true);
    CHECK(voice(effect).paused && !voice(line).paused && audio::paused(AudioGroup::Effects));
    const Voice during = audio::play(sound); // starts paused while its group is
    CHECK(voice(during).paused);
    audio::set_paused(effect, true);
    audio::set_paused(AudioGroup::Effects, false);
    CHECK(voice(effect).paused && !voice(during).paused); // its own pause remains
    nnh::shutdown();
}

NNH_TEST(a_full_mixer_replaces_or_drops) {
    nnh::Sound sound = start({.audio = {.voices = 2}});
    g_finished = 0;
    auto sub = nnh::events::subscribe<nnh::SoundFinished>([](const nnh::SoundFinished&) { ++g_finished; });
    const Voice first = audio::play(sound, {.priority = 10});
    const Voice second = audio::play(sound, {.priority = 20});
    const Voice third = audio::play(sound, {.priority = 10}); // replaces `first` (lowest, oldest)
    CHECK(third.generation != 0 && third.index == first.index);
    CHECK(!audio::playing(first) && audio::playing(second) && audio::playing(third));
    CHECK(voice(third).starts == 2 && voice(third).stops == 1);
    const int lines = fake::state().console_lines;
    const Voice dropped = audio::play(sound, {.priority = 5});
    CHECK(dropped.generation == 0);
    CHECK(fake::state().console_lines == lines + 1); // logged...
    audio::play(sound, {.priority = 5});
    CHECK(fake::state().console_lines == lines + 1); // ...once per sound
    CHECK(nnh::frame());
    CHECK(g_finished == 0); // a replaced voice is not "finished"
    nnh::shutdown();
}

NNH_TEST(playing_an_empty_sound_does_nothing) {
    nnh::Sound sound = start();
    nnh::Sound empty;
    CHECK(audio::play(empty).generation == 0);
    sound.reset();
    CHECK(audio::play(sound).generation == 0);
    nnh::shutdown();
    CHECK(audio::play(sound).generation == 0);
}

NNH_TEST(resetting_a_sound_stops_its_voices) {
    nnh::Sound sound = start();
    const Voice v = audio::play(sound, {.loop = true});
    sound.reset(); // the fake aborts if a sample is freed while a voice plays it
    CHECK(!audio::playing(v) && !voice(v).playing && fake::state().live_samples == 0);
    nnh::shutdown();
}
