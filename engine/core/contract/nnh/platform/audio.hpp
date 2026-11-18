#pragma once
/// @file nnh/platform/audio.hpp
/// Platform contract — audio output (spec 008, decision D21). Implemented by every platform library,
/// called only by nnh::core. Not for games. See docs/architecture/platforms.md.
///
/// The platform plays **voices**: a sample (16-bit frames in memory it owns) at a rate, with a gain per
/// speaker, once or looping. Core decides everything else (which voice plays what, priorities, groups,
/// pan, pitch) and only calls these with valid arguments: voice in [0, voices), a live sample.

#include <cstddef>
#include <cstdint>

namespace nnh::platform {

/// A sample as the platform knows it (0 = none).
using SampleHandle = std::uintptr_t;

/// What core asks of a playing voice.
struct VoiceParams {
    float gain_left = 1.0f;  ///< 0..1
    float gain_right = 1.0f; ///< 0..1
    float rate = 0.0f;       ///< frames per second to play (sample rate × pitch)
};

/// Memory for a sample, for core to fill.
struct SampleInfo {
    SampleHandle handle = 0;
    std::int16_t* frames = nullptr; ///< `count × channels` interleaved values to write
    std::size_t bytes = 0;          ///< memory it uses (core accounts it in the Linear region)
};

/// Starts audio output with `voices` voices (1–22). @return false if the system has no audio output
/// (set last_error()): core then runs silently and calls none of the functions below except
/// audio_shutdown(). Called once by nnh::init().
bool audio_init(int voices);
void audio_shutdown();

/// Allocates a sample of `count` frames of `channels` (1 or 2) in memory the audio hardware reads
/// (3DS: linear memory). Load time only. @return false (out.handle 0) if out of memory.
bool sample_create(std::uint32_t count, int channels, SampleInfo& out);
/// Core wrote the frames: make them visible to the audio hardware (3DS: flush the data cache).
void sample_ready(SampleHandle sample);
/// Frees a sample. Core stops every voice playing it first.
void sample_destroy(SampleHandle sample);

/// Plays `count` frames of `channels` from `sample` on `voice` (replacing what it played), from the
/// start, once or looping. Must not allocate.
void voice_start(int voice, SampleHandle sample, std::uint32_t count, int channels, bool loop, const VoiceParams& params);
void voice_update(int voice, const VoiceParams& params);
void voice_pause(int voice, bool paused);
void voice_stop(int voice);
/// True from voice_start() until a non-looping voice played its last frame or voice_stop(); a paused
/// voice is playing.
bool voice_playing(int voice);

// Streams (music): PCM the core decodes while the game runs, queued in fixed buffers the platform owns
// (allocated by audio_init(): kStreams x kStreamBuffers x kStreamBufferFrames stereo frames, counted by
// core under `audio` in the Linear region as kStreamBytes). Played after the voices' channels.
inline constexpr int kStreams = 2; // a crossfade plays two tracks
inline constexpr int kStreamBuffers = 4;
inline constexpr std::uint32_t kStreamBufferFrames = 4096; // ~0.13 s at 32 kHz; 4 queued = ~0.5 s
inline constexpr std::size_t kStreamBytes = std::size_t{kStreams} * kStreamBuffers * kStreamBufferFrames * 2 * 2;

/// Starts stream `stream` (0..kStreams-1) empty, for `channels` (1 or 2) at `params`. Replaces what it played.
void stream_start(int stream, int channels, const VoiceParams& params);
/// A free buffer to fill (`capacity` = kStreamBufferFrames frames of the stream's channels), or nullptr
/// if all are queued. Must not allocate.
std::int16_t* stream_buffer(int stream, std::uint32_t& capacity);
/// Queues the buffer stream_buffer() returned, holding `frames` (1..capacity) frames, after the others.
void stream_submit(int stream, std::uint32_t frames);
void stream_update(int stream, const VoiceParams& params);
void stream_pause(int stream, bool paused);
/// Stops a stream and drops what is queued.
void stream_stop(int stream);
/// Frames queued and not played yet (0 = it played everything it was given).
std::uint32_t stream_queued(int stream);

} // namespace nnh::platform
