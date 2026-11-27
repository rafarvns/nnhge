// The music kind of hot reload (spec 031 T5.3, FR-9): a playing track whose file is reloaded goes on from the same
// sample in the new file (the queued buffers of the old one play out first), or from the loop start when the new file is
// shorter; a fade keeps its progress; a damaged file refuses the batch and the old track plays on.

#include "../reload/reload_fixture.hpp"
#include "audio_test_util.hpp"
#include "nnh/audio.hpp"

using reload_test::Put;

namespace {

namespace fake = nnh::platform_fake;

constexpr std::uint32_t kFrames = 40000;
std::uint8_t g_a[64 + kFrames * 2];
std::uint8_t g_b[64 + kFrames * 2];
std::uint8_t g_short[64 + 10000 * 2];
std::size_t g_a_size = 0, g_b_size = 0, g_short_size = 0;

std::int16_t a_value(std::uint32_t i) { return static_cast<std::int16_t>(i / 2); }
std::int16_t b_value(std::uint32_t i) { return static_cast<std::int16_t>(-static_cast<int>(i / 2) - 1); }

void start_music(char* index, std::size_t capacity) {
    g_a_size = audio_test::make_wav(g_a, 1, 16, 8000, kFrames, [](std::uint32_t i) { return a_value(i); });
    g_b_size = audio_test::make_wav(g_b, 1, 16, 8000, kFrames, [](std::uint32_t i) { return b_value(i); });
    g_short_size = audio_test::make_wav(g_short, 1, 16, 8000, 10000, [](std::uint32_t i) { return b_value(i); });
    const Put assets[] = {Put{"music/a.wav", g_a, g_a_size}};
    reload_test::start(assets, 1, index, capacity);
}

/// Sends `file` as music/a.wav (batch `id`) and runs frames until the batch is applied or refused.
void reload_with(const void* file, std::size_t size, std::uint16_t id = 1) {
    const Put batch[] = {Put{"music/a.wav", file, size}};
    const std::uint32_t applied = nnh::assets::reload_count();
    const int refused = reload_test::records().refused;
    reload_test::send_batch(id, batch, 1);
    for (int i = 0; i < 20 && nnh::assets::reload_count() == applied && reload_test::records().refused == refused; ++i) {
        CHECK(nnh::frame());
    }
}

std::int16_t logged(std::uint32_t at) {
    std::uint32_t frames = 0;
    const std::int16_t* log = fake::stream_log(0, frames);
    return at < frames ? log[at] : static_cast<std::int16_t>(0x7FFF);
}

} // namespace

NNH_TEST(music_reload_goes_on_from_the_same_sample_in_the_new_file) {
    RELOAD_DEV_ONLY();
    char index[64];
    start_music(index, sizeof index);
    CHECK(nnh::music::play("music/a", {.loop = false}).ok());
    fake::consume_stream(0, 4096);
    CHECK(nnh::frame()); // one buffer refilled: 20480 frames decoded, 4096 played
    const float before = nnh::music::position();
    reload_with(g_b, g_b_size);
    CHECK(nnh::assets::reload_count() == 1);
    CHECK(nnh::music::playing());
    CHECK(nnh::music::position() == before);
    fake::consume_stream(0, 4096);
    CHECK(nnh::frame());
    CHECK(logged(20479) == a_value(20479)); // the old file's queued samples
    CHECK(logged(20480) == b_value(20480)); // then the new file, from the same sample
    nnh::shutdown();
}

NNH_TEST(music_reload_past_the_new_end_goes_on_from_the_loop_start) {
    RELOAD_DEV_ONLY();
    char index[64];
    start_music(index, sizeof index);
    CHECK(nnh::music::play("music/a", {.loop = true}).ok());
    fake::consume_stream(0, 4096);
    CHECK(nnh::frame());
    reload_with(g_short, g_short_size);
    CHECK(nnh::assets::reload_count() == 1);
    fake::consume_stream(0, 4096);
    CHECK(nnh::frame());
    CHECK(logged(20480) == b_value(0));
    nnh::shutdown();
}

NNH_TEST(music_reload_of_an_ogg_track_keeps_its_place) {
    RELOAD_DEV_ONLY();
    static std::uint8_t ogg[16 * 1024];
    const std::size_t size = audio_test::load_fixture("sweep_stereo_32k_loop.ogg", ogg, sizeof ogg);
    CHECK(size > 0);
    char index[64];
    const Put assets[] = {Put{"music/song.ogg", ogg, size}};
    reload_test::start(assets, 1, index, sizeof index);
    CHECK(nnh::music::play("music/song").ok());
    std::uint32_t frames = 0;
    fake::consume_stream(0, 4100);
    CHECK(nnh::frame());
    const float before = nnh::music::position();
    const Put batch[] = {Put{"music/song.ogg", ogg, size}};
    reload_test::send_batch(1, batch, 1);
    for (int i = 0; i < 10 && nnh::assets::reload_count() == 0; ++i) CHECK(nnh::frame());
    CHECK(nnh::assets::reload_count() == 1);
    CHECK(nnh::music::playing() && nnh::music::position() == before);
    const std::int16_t* log = fake::stream_log(0, frames);
    const std::uint32_t decoded = frames;
    fake::consume_stream(0, 4100);
    CHECK(nnh::frame());
    log = fake::stream_log(0, frames);
    CHECK(frames > decoded); // the reopened decoder refills the freed buffer
    CHECK(log != nullptr);
    nnh::shutdown();
}

NNH_TEST(music_reload_keeps_a_fade_and_a_damaged_file_keeps_the_old_track) {
    RELOAD_DEV_ONLY();
    char index[64];
    start_music(index, sizeof index);
    CHECK(nnh::music::play("music/a", {.loop = false, .fade_in = 2.0f}).ok());
    fake::advance_time(nnh::Duration{500'000'000});
    CHECK(nnh::frame());
    const float gain = fake::state().streams[0].params.gain_left;
    CHECK(gain > 0.0f && gain < 1.0f);
    static std::uint8_t bad[64];
    std::memcpy(bad, g_b, sizeof bad);
    std::memcpy(bad + 8, "JUNK", 4); // not a WAVE file any more
    reload_with(bad, sizeof bad);
    CHECK(nnh::assets::reload_count() == 0);
    CHECK(reload_test::records().refused == 1);
    reload_with(g_b, g_b_size, 2);
    CHECK(nnh::assets::reload_count() == 1);
    CHECK(fake::state().streams[0].params.gain_left == gain); // no time passed: the fade where it was
    nnh::shutdown();
}

NNH_TEST(music_a_stream_whose_file_is_rewritten_underneath_never_crashes) {
    RELOAD_DEV_ONLY();
    char index[64];
    start_music(index, sizeof index);
    CHECK(nnh::music::play("music/a", {.loop = true}).ok());
    // The build rewrites the packed file the simulator streams (AC-9): shorter, then garbage; no reload involved.
    fake::replace_asset("music/a.wav", g_short, g_short_size);
    for (int i = 0; i < 8; ++i) {
        fake::consume_stream(0, 4096);
        CHECK(nnh::frame());
    }
    static std::uint8_t junk[300];
    for (std::size_t i = 0; i < sizeof junk; ++i) junk[i] = static_cast<std::uint8_t>(i * 37);
    fake::replace_asset("music/a.wav", junk, sizeof junk);
    for (int i = 0; i < 8; ++i) {
        fake::consume_stream(0, 4096);
        CHECK(nnh::frame());
    }
    nnh::music::stop();
    CHECK(nnh::frame());
    nnh::shutdown();
}

NNH_TEST(music_reload_keeps_a_crossfade_in_progress) {
    RELOAD_DEV_ONLY();
    char index[64];
    start_music(index, sizeof index);
    CHECK(nnh::music::play("music/a", {.loop = true}).ok());
    CHECK(nnh::music::crossfade("music/a", 2.0f, {.loop = true}).ok()); // both streams read music/a.wav
    fake::advance_time(nnh::Duration{500'000'000});
    CHECK(nnh::frame());
    const float out_gain = fake::state().streams[0].params.gain_left;
    const float in_gain = fake::state().streams[1].params.gain_left;
    CHECK(out_gain > in_gain && in_gain > 0.0f);
    reload_with(g_b, g_b_size);
    CHECK(nnh::assets::reload_count() == 1);
    CHECK(fake::state().streams[0].active && fake::state().streams[1].active); // both reopened, none ended
    CHECK(fake::state().streams[0].params.gain_left == out_gain); // no time passed: the crossfade where it was
    CHECK(fake::state().streams[1].params.gain_left == in_gain);
    nnh::shutdown();
}
