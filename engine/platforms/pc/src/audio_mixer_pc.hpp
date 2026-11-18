#pragma once
// The PC simulator's software mixer (spec 008, decision D21): does what the 3DS DSP does in hardware —
// voices (16-bit samples at any rate, gain per speaker, loop, pause) and streams (queued buffers) mixed
// into 48 kHz stereo floats with linear resampling. Gain changes are ramped over the next block so they
// don't click. No SDL here: audio_pc.cpp calls it under the audio stream's lock. Unit tested in
// tests/simulator/test_audio_mixer_pc.cpp.

#include <cstdint>

#include "nnh/platform/audio.hpp"

namespace nnh::platform::pc {

inline constexpr int kMixerVoices = 22;
inline constexpr float kOutputRate = 48000.0f;

/// A sample the platform created (audio_pc.cpp owns the memory).
struct PcSample {
    const std::int16_t* frames = nullptr;
    std::uint32_t count = 0;
    int channels = 1;
};

class PcMixer {
public:
    /// Silences and forgets every voice and stream.
    void reset();

    void voice_start(int voice, const PcSample* sample, bool loop, const platform::VoiceParams& params);
    void voice_update(int voice, const platform::VoiceParams& params);
    void voice_pause(int voice, bool paused);
    void voice_stop(int voice);
    bool voice_playing(int voice) const;
    bool sample_in_use(const PcSample* sample) const;

    void stream_start(int stream, int channels, const platform::VoiceParams& params);
    std::int16_t* stream_buffer(int stream, std::uint32_t& capacity);
    void stream_submit(int stream, std::uint32_t frames);
    void stream_update(int stream, const platform::VoiceParams& params);
    void stream_pause(int stream, bool paused);
    void stream_stop(int stream);
    std::uint32_t stream_queued(int stream) const;

    /// Writes `frames` stereo frames (left, right, ...) to `out`, each value clamped to [-1, 1].
    void mix(float* out, std::uint32_t frames);

private:
    struct Gains {
        float left = 0;
        float right = 0;
    };
    struct Voice {
        const PcSample* sample = nullptr;
        bool playing = false;
        bool paused = false;
        bool loop = false;
        double position = 0; ///< in sample frames
        double step = 1;     ///< sample frames per output frame
        Gains gain, target;
    };
    struct Stream {
        bool active = false;
        bool paused = false;
        int channels = 1;
        double position = 0; ///< in frames of the front buffer
        double step = 1;
        Gains gain, target;
        std::int16_t data[platform::kStreamBuffers][platform::kStreamBufferFrames * 2];
        std::uint32_t frames[platform::kStreamBuffers] = {};
        int order[platform::kStreamBuffers] = {}; ///< queued buffers, oldest first
        int queued = 0;
        int handed_out = -1;
    };

    void mix_voice(Voice& v, float* out, std::uint32_t frames);
    void mix_stream(Stream& s, float* out, std::uint32_t frames);
    /// Frame `offset` frames after the front of a stream's queue (channel `c`), or 0 past the end.
    static float stream_value(const Stream& s, std::uint32_t offset, int c);

    Voice voices_[kMixerVoices];
    Stream streams_[platform::kStreams];
};

} // namespace nnh::platform::pc
