// The sound kind of hot reload (spec 031 T5.2, FR-7, FR-9, Q7): a reloaded WAV keeps the game's Sound handle with the
// new samples (duration); voices playing it stop with SoundFinished in that frame; the next play uses the new samples;
// a broken file refuses the batch and keeps the old sound.

#include <cstring>

#include "../reload/reload_fixture.hpp"
#include "audio/audio_internal.hpp"
#include "audio_test_util.hpp"
#include "nnh/audio.hpp"
#include "nnh/events.hpp"

using reload_test::Put;

namespace {

std::uint8_t g_short[64 + 8000 * 2];
std::uint8_t g_long[64 + 16000 * 2];
std::size_t g_short_size = 0, g_long_size = 0;

void start_sounds(char* index, std::size_t capacity) {
    g_short_size = audio_test::make_wav(g_short, 1, 16, 8000, 8000, [](std::uint32_t) { return 100; });
    g_long_size = audio_test::make_wav(g_long, 1, 16, 8000, 16000, [](std::uint32_t) { return 200; });
    const Put assets[] = {Put{"sfx/blip.wav", g_short, g_short_size}};
    reload_test::start(assets, 1, index, capacity);
}

} // namespace

NNH_TEST(sound_reload_same_handle_new_samples_and_voices_stopped) {
    RELOAD_DEV_ONLY();
    char index[64];
    start_sounds(index, sizeof index);
    {
        auto loaded = nnh::load_sound("sfx/blip");
        CHECK(loaded.ok());
        nnh::Sound blip = std::move(loaded.value());
        CHECK(blip.duration() == 1.0f);
        const nnh::platform::SampleHandle before = nnh::detail::sound_sample(blip);
        const nnh::Voice v = nnh::audio::play(blip);
        struct Seen {
            int finished = 0;
            nnh::Voice last;
        } seen;
        auto sub = nnh::events::subscribe<nnh::SoundFinished>([&seen](const nnh::SoundFinished& e) {
            ++seen.finished;
            seen.last = e.voice;
        });
        CHECK(nnh::frame());
        CHECK(seen.finished == 0);
        const Put batch[] = {Put{"sfx/blip.wav", g_long, g_long_size}};
        reload_test::send_batch(1, batch, 1);
        for (int i = 0; i < 5 && nnh::assets::reload_count() == 0; ++i) CHECK(nnh::frame()); // 32 KB: a few frames
        CHECK(nnh::assets::reload_count() == 1);
        CHECK(seen.finished == 1 && seen.last == v); // stopped at the reload point, in that frame
        CHECK(blip.valid());
        CHECK(blip.duration() == 2.0f);
        CHECK(nnh::detail::sound_sample(blip) != before);
        const nnh::Voice again = nnh::audio::play(blip);
        CHECK(again.generation != 0); // the next play uses the new samples
    }
    nnh::shutdown();
}

NNH_TEST(sound_reload_a_broken_file_keeps_the_old_sound) {
    RELOAD_DEV_ONLY();
    char index[64];
    start_sounds(index, sizeof index);
    {
        nnh::Sound blip = std::move(nnh::load_sound("sfx/blip").value());
        static const char kBroken[] = "RIFF....not a wave";
        const Put batch[] = {Put{"sfx/blip.wav", kBroken, sizeof kBroken - 1}};
        reload_test::send_batch(1, batch, 1);
        CHECK(nnh::frame());
        CHECK(nnh::assets::reload_count() == 0);
        CHECK(blip.duration() == 1.0f);
    }
    nnh::shutdown();
}
