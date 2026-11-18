// PC simulator: audio output through SDL 3 (contract nnh/platform/audio.hpp, spec 008). One playback
// stream (48 kHz stereo float) whose callback runs PcMixer, the software stand-in for the 3DS DSP. SDL
// holds the stream's lock during the callback; every call from core takes the same lock.

#include <SDL3/SDL.h>

#include <cstdlib>
#include <new>

#include "audio_mixer_pc.hpp"
#include "context_pc.hpp"
#include "nnh/platform/audio.hpp"

namespace nnh::platform {
namespace {

pc::PcMixer g_mixer;            // ~130 KB of stream buffers: static
SDL_AudioStream* g_stream = nullptr;
float g_block[2 * 1024];        // one mix block, used only inside the callback

void SDLCALL feed(void*, SDL_AudioStream* stream, int additional, int) {
    // `additional` bytes are needed now; mix them in blocks.
    int frames = additional / static_cast<int>(2 * sizeof(float));
    while (frames > 0) {
        const int n = frames < 1024 ? frames : 1024;
        g_mixer.mix(g_block, static_cast<std::uint32_t>(n));
        SDL_PutAudioStreamData(stream, g_block, n * static_cast<int>(2 * sizeof(float)));
        frames -= n;
    }
}

/// Runs `fn` while the audio callback can't.
template <typename Fn>
void locked(Fn&& fn) {
    if (g_stream == nullptr) return;
    SDL_LockAudioStream(g_stream);
    fn();
    SDL_UnlockAudioStream(g_stream);
}

} // namespace

bool audio_init(int) {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        pc::set_error("audio output");
        return false;
    }
    g_mixer.reset();
    const SDL_AudioSpec spec{SDL_AUDIO_F32, 2, static_cast<int>(pc::kOutputRate)};
    g_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, feed, nullptr);
    if (g_stream == nullptr) {
        pc::set_error("no audio device");
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }
    SDL_ResumeAudioStreamDevice(g_stream);
    return true;
}

void audio_shutdown() {
    if (g_stream == nullptr) return;
    SDL_DestroyAudioStream(g_stream); // stops the callback
    g_stream = nullptr;
    g_mixer.reset();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

bool sample_create(std::uint32_t count, int channels, SampleInfo& out) {
    out = {};
    const std::size_t bytes = static_cast<std::size_t>(count) * static_cast<std::size_t>(channels) * sizeof(std::int16_t);
    auto* frames = static_cast<std::int16_t*>(std::malloc(bytes));
    auto* sample = static_cast<pc::PcSample*>(std::malloc(sizeof(pc::PcSample)));
    if (frames == nullptr || sample == nullptr) {
        std::free(frames);
        std::free(sample);
        return false;
    }
    new (sample) pc::PcSample{frames, count, channels};
    out.handle = reinterpret_cast<SampleHandle>(sample);
    out.frames = frames;
    out.bytes = bytes;
    return true;
}

void sample_ready(SampleHandle) {}

void sample_destroy(SampleHandle handle) {
    auto* sample = reinterpret_cast<pc::PcSample*>(handle);
    if (sample == nullptr) return;
    std::free(const_cast<std::int16_t*>(sample->frames));
    std::free(sample);
}

void voice_start(int voice, SampleHandle sample, std::uint32_t, int, bool loop, const VoiceParams& params) {
    locked([&] { g_mixer.voice_start(voice, reinterpret_cast<const pc::PcSample*>(sample), loop, params); });
}

void voice_update(int voice, const VoiceParams& params) {
    locked([&] { g_mixer.voice_update(voice, params); });
}

void voice_pause(int voice, bool paused) {
    locked([&] { g_mixer.voice_pause(voice, paused); });
}

void voice_stop(int voice) {
    locked([&] { g_mixer.voice_stop(voice); });
}

bool voice_playing(int voice) {
    bool playing = false;
    locked([&] { playing = g_mixer.voice_playing(voice); });
    return playing;
}

void stream_start(int stream, int channels, const VoiceParams& params) {
    locked([&] { g_mixer.stream_start(stream, channels, params); });
}

std::int16_t* stream_buffer(int stream, std::uint32_t& capacity) {
    // The buffer returned isn't queued, so the callback doesn't read it while core fills it.
    std::int16_t* buffer = nullptr;
    capacity = 0;
    locked([&] { buffer = g_mixer.stream_buffer(stream, capacity); });
    return buffer;
}

void stream_submit(int stream, std::uint32_t frames) {
    locked([&] { g_mixer.stream_submit(stream, frames); });
}

void stream_update(int stream, const VoiceParams& params) {
    locked([&] { g_mixer.stream_update(stream, params); });
}

void stream_pause(int stream, bool paused) {
    locked([&] { g_mixer.stream_pause(stream, paused); });
}

void stream_stop(int stream) {
    locked([&] { g_mixer.stream_stop(stream); });
}

std::uint32_t stream_queued(int stream) {
    std::uint32_t queued = 0;
    locked([&] { queued = g_mixer.stream_queued(stream); });
    return queued;
}

} // namespace nnh::platform
