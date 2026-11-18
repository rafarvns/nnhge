// Audio without output and across init/shutdown (spec 008 FR-7, FR-8): a system without audio runs
// silently with the same voices and events (timed by the clock), and shutdown frees everything.

#include <cstring>
#include <utility>

#include "audio/audio_internal.hpp"
#include "audio_test_util.hpp"
#include "nnh/audio.hpp"
#include "nnh/events.hpp"
#include "nnh/log.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;
namespace audio = nnh::audio;
using nnh::Voice;

namespace {

std::uint8_t g_wav[64 + 16000];
constexpr char kIndex[] = "sfx/half.wav\n";
int g_finished = 0;

/// A 0.5 s sound (8000 mono frames at 16 kHz).
nnh::Sound start(bool audio_available) {
    nnh::shutdown();
    fake::reset();
    fake::set_audio_available(audio_available);
    const std::size_t size = audio_test::make_wav(g_wav, 1, 16, 16000, 8000, [](std::uint32_t) { return 0; });
    fake::add_asset(".nnh_assets", kIndex, sizeof kIndex - 1);
    fake::add_asset("sfx/half.wav", g_wav, size);
    CHECK(nnh::init());
    auto sound = nnh::load_sound("sfx/half");
    CHECK(sound.ok());
    return sound ? std::move(sound.value()) : nnh::Sound{};
}

/// Runs one frame after `seconds` of clock time.
bool frame_after(double seconds) {
    fake::advance_time(nnh::Duration{static_cast<std::int64_t>(seconds * 1e9)});
    return nnh::frame();
}

} // namespace

NNH_TEST(audio_is_available_on_a_system_with_output) {
    nnh::Sound sound = start(true);
    CHECK(audio::available() && fake::state().audio_initialized);
    CHECK(fake::state().audio_voices == 16); // AppConfig::audio.voices default
    nnh::shutdown();
    CHECK(!fake::state().audio_initialized);
}

NNH_TEST(without_output_the_game_runs_silently_and_says_why_once) {
    nnh::shutdown();
    fake::reset();
    fake::set_audio_available(false);
    nnh::log::reset(); // only this init's lines in the ring
    CHECK(nnh::init());
    CHECK(!audio::available());
    int mentions = 0;
    for (int i = 0; i < nnh::log::ring_size(); ++i) {
        if (std::strstr(nnh::log::ring_line(i), "no audio output") != nullptr &&
            std::strstr(nnh::log::ring_line(i), "fake audio unavailable") != nullptr) {
            ++mentions;
        }
    }
    CHECK(mentions == 1);
    nnh::shutdown();
}

NNH_TEST(without_output_sounds_load_and_voices_end_on_time) {
    nnh::Sound sound = start(false);
    CHECK(sound.valid() && sound.duration() > 0.49f && sound.duration() < 0.51f);
    CHECK(fake::state().live_samples == 0); // nothing handed to the (missing) output
    g_finished = 0;
    auto sub = nnh::events::subscribe<nnh::SoundFinished>([](const nnh::SoundFinished&) { ++g_finished; });
    const Voice v = audio::play(sound);
    const Voice slow = audio::play(sound, {.pitch = 0.5f}); // lasts 1 s
    const Voice looping = audio::play(sound, {.loop = true});
    CHECK(audio::playing(v) && frame_after(0.0));
    CHECK(frame_after(0.3) && audio::playing(v));
    CHECK(frame_after(0.3) && !audio::playing(v) && g_finished == 1);
    CHECK(audio::playing(slow));
    CHECK(frame_after(0.5) && !audio::playing(slow) && g_finished == 2);
    CHECK(frame_after(5.0) && audio::playing(looping)); // loops never end on their own
    nnh::shutdown();
}

NNH_TEST(without_output_a_paused_voice_does_not_advance) {
    nnh::Sound sound = start(false);
    const Voice v = audio::play(sound);
    audio::set_paused(v, true);
    CHECK(frame_after(0.0) && frame_after(2.0) && audio::playing(v));
    audio::set_paused(v, false);
    audio::set_paused(nnh::AudioGroup::Effects, true);
    CHECK(frame_after(2.0) && audio::playing(v)); // its group is paused
    audio::set_paused(nnh::AudioGroup::Effects, false);
    CHECK(frame_after(0.6) && !audio::playing(v));
    nnh::shutdown();
}

NNH_TEST(without_output_pitch_changes_the_time_left) {
    nnh::Sound sound = start(false);
    const Voice v = audio::play(sound);    // 0.5 s
    CHECK(frame_after(0.0) && frame_after(0.25));
    audio::set_pitch(v, 0.5f);             // 0.25 s of sound left takes 0.5 s now
    CHECK(frame_after(0.4) && audio::playing(v));
    CHECK(frame_after(0.2) && !audio::playing(v));
    nnh::shutdown();
}

NNH_TEST(shutdown_stops_voices_and_frees_audio_memory) {
    nnh::Sound sound = start(true);
    const Voice v = audio::play(sound, {.loop = true});
    nnh::shutdown();
    CHECK(!fake::state().voices[v.index].playing);
    CHECK(fake::state().live_samples == 0);
    CHECK(nnh::mem::tag_stats(nnh::detail::audio_memory_tag()).bytes == 0);
    CHECK(!audio::playing(v));
    CHECK(!sound.valid());
}
