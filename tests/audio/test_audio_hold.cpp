// Audio while paused (spec 030 T3.5, Q7): playing voices are held at the platform during paused ticks and released for
// the game frames after (a step plays one frame); a voice the game paused stays paused.

#include "nnh/app.hpp"
#include "nnh/audio.hpp"
#include "nnh/build.hpp"
#include "nnh/platform/system.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;
using nnh::platform::kDevStepFrame;
using nnh::platform::kDevTogglePause;

namespace nnh::detail {
void audio_hold(bool hold);
}

namespace {

// A tiny valid WAV: 16-bit mono, 8 frames.
const unsigned char kWav[] = {'R', 'I', 'F', 'F', 52, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0,
                              1,   0,   1,   0,   0x44, 0xAC, 0, 0, 0x88, 0x58, 1, 0, 2, 0, 16, 0, 'd', 'a', 't', 'a',
                              16,  0,   0,   0,   1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 6, 0, 7, 0, 8, 0};

void restart() {
    nnh::shutdown();
    fake::reset();
    static constexpr char kIndex[] = "sfx/beep.wav\n";
    fake::add_asset(".nnh_assets", kIndex, sizeof kIndex - 1);
    fake::add_asset("sfx/beep.wav", kWav, sizeof kWav);
    CHECK(nnh::init({}));
}

} // namespace

NNH_TEST(audio_hold_pauses_playing_voices_and_restores_the_games_state) {
    if constexpr (!nnh::build::kDevToolsEnabled) return;
    restart();
    auto sound = nnh::load_sound("sfx/beep");
    CHECK(sound.ok());
    if (!sound) return;
    const nnh::Voice a = nnh::audio::play(sound.value(), {.loop = true});
    const nnh::Voice b = nnh::audio::play(sound.value(), {.loop = true});
    nnh::audio::set_paused(b, true); // the game's own pause
    CHECK(nnh::frame());
    nnh::detail::audio_hold(true);
    int paused = 0;
    for (const auto& v : fake::state().voices) paused += v.playing && v.paused ? 1 : 0;
    CHECK(paused == 2);
    nnh::detail::audio_hold(false);
    paused = 0;
    for (const auto& v : fake::state().voices) paused += v.playing && v.paused ? 1 : 0;
    CHECK(paused == 1); // b, which the game paused
    (void)a;
}

NNH_TEST(audio_hold_follows_the_pause_inside_frame) {
    if constexpr (!nnh::build::kDevToolsEnabled) return;
    restart();
    auto sound = nnh::load_sound("sfx/beep");
    CHECK(sound.ok());
    if (!sound) return;
    const nnh::Voice a = nnh::audio::play(sound.value(), {.loop = true});
    CHECK(nnh::frame());
    fake::queue_dev_requests({kDevTogglePause, 0, kDevStepFrame});
    CHECK(nnh::frame()); // paused, then a step: the stepped frame plays
    int paused = 0;
    for (const auto& v : fake::state().voices) paused += v.playing && v.paused ? 1 : 0;
    CHECK(paused == 0);
    (void)a;
}
