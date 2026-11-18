// Streamed music (nnh::music, spec 008 FR-3, FR-4, FR-6, FR-8) through the fake platform: buffers filled
// from the asset, loop points, MusicFinished after the queue drains, pause and volumes, fades and
// crossfades, errors, silence without output, and release at shutdown.

#include <cmath>

#include "audio/audio_internal.hpp"
#include "audio_test_util.hpp"
#include "nnh/audio.hpp"
#include "nnh/events.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;
namespace music = nnh::music;
using nnh::ErrorCode;

namespace {

constexpr std::uint32_t kFrames = 10000; // 1.25 s at 8 kHz
std::uint8_t g_a[64 + kFrames * 2];
std::uint8_t g_b[64 + kFrames * 2];
std::uint8_t g_ogg[16 * 1024];
constexpr char kIndex[] = "music/a.wav\nmusic/b.wav\nmusic/bad.wav\nmusic/song.ogg\n";
int g_finished = 0;

void start(bool audio_available = true) {
    nnh::shutdown();
    fake::reset();
    fake::set_audio_available(audio_available);
    const std::size_t a = audio_test::make_wav(g_a, 1, 16, 8000, kFrames, [](std::uint32_t i) { return static_cast<int>(i); });
    const std::size_t b = audio_test::make_wav(g_b, 1, 16, 8000, kFrames, [](std::uint32_t i) { return -static_cast<int>(i); });
    const std::size_t ogg = audio_test::load_fixture("sweep_stereo_32k_loop.ogg", g_ogg, sizeof g_ogg);
    CHECK(ogg > 0);
    fake::add_asset(".nnh_assets", kIndex, sizeof kIndex - 1);
    fake::add_asset("music/a.wav", g_a, a);
    fake::add_asset("music/b.wav", g_b, b);
    fake::add_asset("music/bad.wav", "RIFF\0\0\0\0WAVEjunk", 16);
    fake::add_asset("music/song.ogg", g_ogg, ogg);
    CHECK(nnh::init());
    g_finished = 0;
}

bool frame_after(double seconds) {
    fake::advance_time(nnh::Duration{static_cast<std::int64_t>(seconds * 1e9)});
    return nnh::frame();
}

const fake::FakeStream& stream(int i) { return fake::state().streams[i]; }
bool near(float a, float b, float tolerance = 1e-4f) { return std::fabs(a - b) < tolerance; }

} // namespace

NNH_TEST(music_streams_a_wav_into_the_platform_buffers) {
    start();
    CHECK(music::play("music/a", {.loop = false}).ok());
    CHECK(music::playing() && stream(0).active && stream(0).channels == 1);
    CHECK(near(stream(0).params.gain_left, 1.0f) && near(stream(0).params.rate, 8000.0f));
    std::uint32_t frames = 0;
    const std::int16_t* log = fake::stream_log(0, frames);
    CHECK(frames == kFrames); // the whole (short) track fits in the 4 buffers
    CHECK(log[0] == 0 && log[1234] == 1234 && log[kFrames - 1] == static_cast<std::int16_t>(kFrames - 1));
    nnh::shutdown();
}

NNH_TEST(music_finishes_once_its_last_frames_have_played) {
    start();
    auto sub = nnh::events::subscribe<nnh::MusicFinished>([](const nnh::MusicFinished&) { ++g_finished; });
    CHECK(music::play("music/a", {.loop = false}).ok());
    CHECK(frame_after(0.0) && g_finished == 0); // queued, not played yet
    fake::consume_stream(0, kFrames - 1);
    CHECK(frame_after(0.1) && g_finished == 0 && music::playing());
    fake::consume_stream(0, 1);
    CHECK(frame_after(0.1) && g_finished == 1 && !music::playing() && !stream(0).active);
    CHECK(frame_after(0.1) && g_finished == 1);
    nnh::shutdown();
}

NNH_TEST(music_loops_back_to_its_loop_point_and_refills) {
    start();
    CHECK(music::play("music/a", {.loop_start = 2500}).ok());
    std::uint32_t frames = 0;
    const std::int16_t* log = fake::stream_log(0, frames);
    CHECK(frames == 4 * 4096);
    CHECK(log[kFrames - 1] == static_cast<std::int16_t>(kFrames - 1) && log[kFrames] == 2500 && log[16383] == 2500 + 6383);
    fake::consume_stream(0, 12000);
    CHECK(frame_after(0.1));
    log = fake::stream_log(0, frames);
    CHECK(frames > 4 * 4096); // emptied buffers were refilled
    // 12000 frames played: the end at 10000, then 2000 after the loop point: frame 4500 = 0.5625 s.
    CHECK(near(music::position(), 4500.0f / 8000.0f));
    nnh::shutdown();
}

NNH_TEST(music_ogg_uses_its_loopstart_tag) {
    start();
    CHECK(music::play("music/song").ok());
    CHECK(stream(0).channels == 2 && near(stream(0).params.rate, 32000.0f));
    for (int played = 0; played < 32000 + 100; played += 4100) { // the queue holds ~16000 frames
        const int n = 32000 + 100 - played < 4100 ? 32000 + 100 - played : 4100;
        fake::consume_stream(0, static_cast<std::uint32_t>(n));
        CHECK(frame_after(0.1)); // refills
    }
    // 100 frames past the end: 100 frames after LOOPSTART=12345.
    CHECK(near(music::position(), (12345.0f + 100.0f) / 32000.0f));
    nnh::shutdown();
}

NNH_TEST(music_pause_and_volumes_reach_the_stream) {
    start();
    CHECK(music::play("music/a").ok());
    music::set_paused(true);
    CHECK(stream(0).paused && music::paused() && music::playing());
    music::set_paused(false);
    nnh::audio::set_paused(nnh::AudioGroup::Music, true);
    CHECK(stream(0).paused);
    nnh::audio::set_paused(nnh::AudioGroup::Music, false);
    CHECK(!stream(0).paused);
    music::set_volume(0.5f);
    CHECK(near(stream(0).params.gain_left, 0.5f));
    nnh::audio::set_volume(nnh::AudioGroup::Music, 0.5f);
    nnh::audio::set_master_volume(0.5f);
    CHECK(near(stream(0).params.gain_right, 0.125f));
    nnh::audio::set_volume(nnh::AudioGroup::Effects, 0.0f); // another group: no effect
    CHECK(near(stream(0).params.gain_right, 0.125f));
    nnh::shutdown();
}

NNH_TEST(music_fades_in_over_time) {
    start();
    CHECK(music::play("music/a", {.fade_in = 1.0f}).ok());
    CHECK(near(stream(0).params.gain_left, 0.0f));
    CHECK(frame_after(0.0) && frame_after(0.5));
    CHECK(near(stream(0).params.gain_left, 0.5f, 1e-3f));
    CHECK(frame_after(0.6));
    CHECK(near(stream(0).params.gain_left, 1.0f));
    nnh::shutdown();
}

NNH_TEST(music_stop_cuts_or_fades_out_without_an_event) {
    start();
    auto sub = nnh::events::subscribe<nnh::MusicFinished>([](const nnh::MusicFinished&) { ++g_finished; });
    CHECK(music::play("music/a").ok());
    music::stop(1.0f);
    CHECK(!music::playing() && stream(0).active); // still audible while fading
    CHECK(frame_after(0.0) && frame_after(0.5));
    CHECK(near(stream(0).params.gain_left, 0.5f, 1e-3f));
    CHECK(frame_after(0.6) && !stream(0).active);
    CHECK(music::play("music/b").ok());
    music::stop();
    CHECK(!stream(0).active && !stream(1).active && g_finished == 0);
    nnh::shutdown();
}

NNH_TEST(music_crossfade_plays_both_tracks_then_only_the_new_one) {
    start();
    auto sub = nnh::events::subscribe<nnh::MusicFinished>([](const nnh::MusicFinished&) { ++g_finished; });
    CHECK(music::play("music/a").ok());
    CHECK(music::crossfade("music/b", 2.0f).ok());
    CHECK(stream(0).active && stream(1).active && music::playing());
    std::uint32_t frames = 0;
    CHECK(fake::stream_log(1, frames)[1] == -1); // the new track is b
    CHECK(frame_after(0.0) && frame_after(0.5));
    CHECK(near(stream(0).params.gain_left, 0.75f, 1e-3f) && near(stream(1).params.gain_left, 0.25f, 1e-3f));
    CHECK(frame_after(1.6));
    CHECK(!stream(0).active && near(stream(1).params.gain_left, 1.0f) && g_finished == 0);
    nnh::shutdown();
}

NNH_TEST(music_play_replaces_the_current_track_at_once) {
    start();
    CHECK(music::play("music/a").ok());
    CHECK(music::play("music/b").ok());
    const int playing_streams = (stream(0).active ? 1 : 0) + (stream(1).active ? 1 : 0);
    CHECK(playing_streams == 1);
    std::uint32_t frames = 0;
    const int live = stream(0).active ? 0 : 1;
    CHECK(fake::stream_log(live, frames)[1] == -1); // b
    nnh::shutdown();
}

NNH_TEST(music_a_track_ending_while_it_fades_out_publishes_nothing) {
    start();
    auto sub = nnh::events::subscribe<nnh::MusicFinished>([](const nnh::MusicFinished&) { ++g_finished; });
    CHECK(music::play("music/a", {.loop = false}).ok());
    CHECK(music::crossfade("music/b", 5.0f).ok());
    fake::consume_stream(0, kFrames); // a reaches its end during the 5 s fade
    CHECK(frame_after(0.0) && frame_after(0.1));
    CHECK(!stream(0).active && g_finished == 0 && music::playing());
    nnh::shutdown();
}

NNH_TEST(music_crossfade_during_a_crossfade_drops_the_oldest) {
    start();
    CHECK(music::play("music/a").ok());
    CHECK(music::crossfade("music/b", 2.0f).ok()); // a fades out on stream 0, b in on stream 1
    CHECK(music::crossfade("music/a", 2.0f).ok()); // a (old) stops at once; b fades out; a fades in on 0
    CHECK(stream(0).active && stream(1).active && stream(0).starts == 2);
    std::uint32_t frames = 0;
    CHECK(fake::stream_log(0, frames)[1] == 1);
    nnh::shutdown();
}

NNH_TEST(music_crossfade_without_music_fades_in) {
    start();
    CHECK(music::crossfade("music/a", 1.0f).ok());
    CHECK(music::playing() && near(stream(0).params.gain_left, 0.0f));
    nnh::shutdown();
}

NNH_TEST(music_errors_leave_the_current_track_playing) {
    start();
    CHECK(music::play("music/a").ok());
    CHECK(music::play("music/missing").error().code == ErrorCode::NotFound);
    CHECK(music::play("music/bad").error().code == ErrorCode::Corrupted);
    CHECK(music::play("music/b", {.loop_start = kFrames}).error().code == ErrorCode::InvalidArgument);
    CHECK(music::play("../x").error().code == ErrorCode::InvalidArgument);
    CHECK(music::crossfade("music/missing", 1.0f).error().code == ErrorCode::NotFound);
    CHECK(music::playing() && stream(0).active);
    std::uint32_t frames = 0;
    CHECK(fake::stream_log(0, frames)[1] == 1); // still a
    nnh::shutdown();
    CHECK(music::play("music/a").error().code == ErrorCode::Unsupported);
    CHECK(!music::playing() && music::position() == 0.0f);
}

NNH_TEST(music_without_output_keeps_time_and_finishes) {
    start(false);
    auto sub = nnh::events::subscribe<nnh::MusicFinished>([](const nnh::MusicFinished&) { ++g_finished; });
    CHECK(music::play("music/a", {.loop = false}).ok());
    CHECK(music::playing() && !stream(0).active);
    CHECK(frame_after(0.0) && frame_after(0.5));
    CHECK(near(music::position(), 0.5f, 1e-3f));
    music::set_paused(true);
    CHECK(frame_after(3.0) && near(music::position(), 0.5f, 1e-3f));
    music::set_paused(false);
    CHECK(frame_after(0.8) && g_finished == 1 && !music::playing());
    CHECK(music::play("music/a", {.loop_start = 2000}).ok());
    CHECK(frame_after(0.0) && frame_after(1.5)); // 12000 frames: 2000 past the end -> 4000
    CHECK(near(music::position(), 0.5f, 1e-3f));
    nnh::shutdown();
}

NNH_TEST(music_memory_and_files_are_released) {
    start();
    nnh::MemTag& tag = nnh::detail::audio_memory_tag();
    const std::size_t idle = nnh::mem::tag_stats(tag).bytes;
    CHECK(idle >= nnh::platform::kStreamBytes); // the platform's stream buffers
    CHECK(music::play("music/song").ok());
    CHECK(nnh::mem::tag_stats(tag).bytes > idle); // + the Ogg decoder's arena
    CHECK(fake::state().open_asset_streams == 1);
    CHECK(music::crossfade("music/a", 1.0f).ok());
    CHECK(fake::state().open_asset_streams == 2);
    music::stop();
    CHECK(frame_after(0.0) && frame_after(1.5));
    CHECK(fake::state().open_asset_streams == 0 && nnh::mem::tag_stats(tag).bytes == idle);
    CHECK(music::play("music/song").ok());
    nnh::shutdown();
    CHECK(fake::state().open_asset_streams == 0 && nnh::mem::tag_stats(tag).bytes == 0);
}
