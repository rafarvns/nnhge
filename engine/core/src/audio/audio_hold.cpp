// Audio while paused (spec 030 Q7, D52): every playing voice and stream is held at the platform during paused ticks and
// released for a game frame (a step plays one frame of audio), without touching the game's own pause flags.
// Debug/Development only (app/dev_pause.cpp). Tests: tests/audio/test_audio_hold.cpp.

#include "audio/audio_internal.hpp"

namespace nnh::detail {

void audio_hold(bool hold) {
    AudioState& a = audio_state();
    if (!a.ready || !a.available) return;
    if (hold) {
        for_each_voice([](int i) { platform::voice_pause(i, true); });
        if (a.music != nullptr) {
            for (int s = 0; s < platform::kStreams; ++s) platform::stream_pause(s, true); // a stopped one restarts unpaused
        }
        return;
    }
    for_each_voice([&](int i) {
        const VoiceState& v = a.voices.at(i);
        platform::voice_pause(i, a.mixer.paused(v.paused, v.group)); // the game's own state again
    });
    if (a.music != nullptr && a.music->mix_changed != nullptr) a.music->mix_changed(); // re-sends the streams' pause
}

} // namespace nnh::detail
