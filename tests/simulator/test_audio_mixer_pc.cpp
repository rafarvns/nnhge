// The PC simulator's software mixer (engine/platforms/pc/src/audio_mixer_pc.hpp, spec 008 FR-9): voices
// and streams resampled to 48 kHz stereo, gains ramped per block, loop, pause, end, stream queues.

#include <cmath>

#include "audio_mixer_pc.hpp"
#include "test.hpp"

using nnh::platform::pc::PcMixer;
using nnh::platform::pc::PcSample;
using nnh::platform::VoiceParams;

namespace {

PcMixer g_mixer;
float g_out[2 * 4096];
bool near(float a, float b, float tolerance = 1e-4f) { return std::fabs(a - b) < tolerance; }
constexpr float k = 1.0f / 32768.0f;

} // namespace

NNH_TEST(pc_mixer_plays_a_voice_at_the_output_rate) {
    static const std::int16_t frames[] = {1000, 2000, 3000, 4000};
    static const PcSample sample{frames, 4, 1};
    g_mixer.reset();
    g_mixer.voice_start(0, &sample, false, VoiceParams{1.0f, 0.5f, 48000.0f});
    g_mixer.mix(g_out, 6);
    CHECK(near(g_out[0], 1000 * k) && near(g_out[1], 500 * k)); // mono to both, right at half gain
    CHECK(near(g_out[6], 4000 * k));
    CHECK(near(g_out[8], 0.0f) && near(g_out[11], 0.0f)); // ended: silence
    CHECK(!g_mixer.voice_playing(0));
}

NNH_TEST(pc_mixer_resamples_linearly) {
    static const std::int16_t frames[] = {0, 1000, 2000};
    static const PcSample sample{frames, 3, 1};
    g_mixer.reset();
    g_mixer.voice_start(0, &sample, false, VoiceParams{1.0f, 1.0f, 24000.0f}); // half speed
    g_mixer.mix(g_out, 4);
    CHECK(near(g_out[0], 0.0f) && near(g_out[2], 500 * k) && near(g_out[4], 1000 * k) && near(g_out[6], 1500 * k));
    CHECK(g_mixer.voice_playing(0));
}

NNH_TEST(pc_mixer_stereo_samples_keep_their_sides_and_loops_wrap) {
    static const std::int16_t frames[] = {100, -100, 200, -200};
    static const PcSample sample{frames, 2, 2};
    g_mixer.reset();
    g_mixer.voice_start(3, &sample, true, VoiceParams{1.0f, 1.0f, 48000.0f});
    g_mixer.mix(g_out, 5);
    CHECK(near(g_out[0], 100 * k) && near(g_out[1], -100 * k) && near(g_out[2], 200 * k));
    CHECK(near(g_out[4], 100 * k) && near(g_out[8], 100 * k)); // frames 2 and 4 wrapped to 0
    CHECK(g_mixer.voice_playing(3));
}

NNH_TEST(pc_mixer_ramps_gain_changes_over_a_block) {
    static std::int16_t frames[64];
    for (auto& f : frames) f = 16384;
    static const PcSample sample{frames, 64, 1};
    g_mixer.reset();
    g_mixer.voice_start(0, &sample, true, VoiceParams{1.0f, 1.0f, 48000.0f});
    g_mixer.mix(g_out, 8);
    g_mixer.voice_update(0, VoiceParams{0.0f, 0.0f, 48000.0f});
    g_mixer.mix(g_out, 8);
    CHECK(g_out[0] > 0.4f);                 // starts near the old gain...
    CHECK(g_out[0] > g_out[8] && g_out[8] > g_out[14]); // ...falls steadily...
    g_mixer.mix(g_out, 8);
    CHECK(near(g_out[0], 0.0f));            // ...and holds the new one
}

NNH_TEST(pc_mixer_pause_keeps_the_position_and_stop_ends) {
    static const std::int16_t frames[] = {10, 20, 30, 40};
    static const PcSample sample{frames, 4, 1};
    g_mixer.reset();
    g_mixer.voice_start(1, &sample, false, VoiceParams{1.0f, 1.0f, 48000.0f});
    g_mixer.mix(g_out, 1);
    g_mixer.voice_pause(1, true);
    g_mixer.mix(g_out, 2);
    CHECK(near(g_out[0], 0.0f) && g_mixer.voice_playing(1));
    g_mixer.voice_pause(1, false);
    g_mixer.mix(g_out, 1);
    CHECK(near(g_out[0], 20 * k));
    CHECK(g_mixer.sample_in_use(&sample));
    g_mixer.voice_stop(1);
    CHECK(!g_mixer.voice_playing(1) && !g_mixer.sample_in_use(&sample));
}

NNH_TEST(pc_mixer_adds_voices_and_clamps) {
    static const std::int16_t loud[] = {30000, 30000};
    static const PcSample sample{loud, 2, 1};
    g_mixer.reset();
    g_mixer.voice_start(0, &sample, true, VoiceParams{1.0f, 1.0f, 48000.0f});
    g_mixer.voice_start(1, &sample, true, VoiceParams{1.0f, 1.0f, 48000.0f});
    g_mixer.mix(g_out, 1);
    CHECK(near(g_out[0], 1.0f)); // 60000/32768 clamped
}

NNH_TEST(pc_mixer_streams_play_their_queue_in_order) {
    g_mixer.reset();
    g_mixer.stream_start(0, 1, VoiceParams{1.0f, 1.0f, 48000.0f});
    std::uint32_t capacity = 0;
    for (int b = 0; b < 2; ++b) {
        std::int16_t* buffer = g_mixer.stream_buffer(0, capacity);
        CHECK(buffer != nullptr && capacity == nnh::platform::kStreamBufferFrames);
        if (buffer == nullptr) return;
        for (int i = 0; i < 3; ++i) buffer[i] = static_cast<std::int16_t>(100 * (b * 3 + i + 1));
        g_mixer.stream_submit(0, 3);
    }
    CHECK(g_mixer.stream_queued(0) == 6);
    g_mixer.mix(g_out, 4);
    CHECK(near(g_out[0], 100 * k) && near(g_out[4], 300 * k) && near(g_out[6], 400 * k));
    CHECK(g_mixer.stream_queued(0) == 2);
    g_mixer.mix(g_out, 4); // runs dry: silence
    CHECK(near(g_out[2], 600 * k) && near(g_out[4], 0.0f) && g_mixer.stream_queued(0) == 0);
}

NNH_TEST(pc_mixer_stream_buffers_run_out_until_played) {
    g_mixer.reset();
    g_mixer.stream_start(1, 2, VoiceParams{1.0f, 1.0f, 48000.0f});
    std::uint32_t capacity = 0;
    for (int b = 0; b < nnh::platform::kStreamBuffers; ++b) {
        CHECK(g_mixer.stream_buffer(1, capacity) != nullptr);
        g_mixer.stream_submit(1, 10);
    }
    CHECK(g_mixer.stream_buffer(1, capacity) == nullptr); // all queued
    g_mixer.mix(g_out, 10);
    CHECK(g_mixer.stream_buffer(1, capacity) != nullptr); // the first one played
    g_mixer.stream_pause(1, true);
    g_mixer.mix(g_out, 10);
    CHECK(g_mixer.stream_queued(1) == 30);
    g_mixer.stream_stop(1);
    CHECK(g_mixer.stream_queued(1) == 0);
}
