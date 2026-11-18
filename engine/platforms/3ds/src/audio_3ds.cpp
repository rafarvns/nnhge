// Nintendo 3DS: audio output on the DSP through libctru's ndsp (contract nnh/platform/audio.hpp, spec 008,
// decision D21). The DSP mixes 24 channels in hardware: voices use channels 0..21, the two music streams
// 22 and 23. Samples and stream buffers live in linear memory (the DSP reads it); the data cache is
// flushed before the DSP sees new data. ndspInit() fails without the DSP firmware the user dumps once
// (sdmc:/3ds/dspfirm.cdc): core then runs silently.

#include <3ds.h>

#include <cstdlib>
#include <cstring>

#include "context_3ds.hpp"
#include "nnh/platform/audio.hpp"

namespace nnh::platform {
namespace {

constexpr int kFirstStreamChannel = 22;

struct Sample3ds {
    std::int16_t* frames;
    std::uint32_t bytes;
};

bool g_ready = false;
ndspWaveBuf g_voice_buffers[kFirstStreamChannel];
bool g_voice_active[kFirstStreamChannel] = {};

struct Stream3ds {
    ndspWaveBuf buffers[kStreamBuffers];
    int handed_out = -1;
    int channels = 1;
};
Stream3ds g_streams[kStreams];
std::int16_t* g_stream_memory = nullptr; // kStreamBytes of linear memory

void set_mix(int channel, const VoiceParams& params) {
    float mix[12] = {};
    mix[0] = params.gain_left;
    mix[1] = params.gain_right;
    ndspChnSetMix(channel, mix);
    ndspChnSetRate(channel, params.rate);
}

void setup_channel(int channel, int channels, const VoiceParams& params) {
    ndspChnReset(channel);
    ndspChnSetInterp(channel, NDSP_INTERP_LINEAR);
    ndspChnSetFormat(channel, channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
    set_mix(channel, params);
}

bool queued(const ndspWaveBuf& b) { return b.status == NDSP_WBUF_QUEUED || b.status == NDSP_WBUF_PLAYING; }

} // namespace

bool audio_init(int) {
    if (R_FAILED(ndspInit())) {
        ctr::set_error("ndspInit failed: the DSP firmware (sdmc:/3ds/dspfirm.cdc) is missing");
        return false;
    }
    g_stream_memory = static_cast<std::int16_t*>(linearAlloc(kStreamBytes));
    if (g_stream_memory == nullptr) {
        ndspExit();
        ctr::set_error("no linear memory for the audio streams");
        return false;
    }
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspSetMasterVol(1.0f);
    std::memset(g_voice_buffers, 0, sizeof g_voice_buffers);
    for (bool& a : g_voice_active) a = false;
    for (int s = 0; s < kStreams; ++s) {
        std::memset(g_streams[s].buffers, 0, sizeof g_streams[s].buffers);
        for (int b = 0; b < kStreamBuffers; ++b) {
            const std::size_t at = (static_cast<std::size_t>(s) * kStreamBuffers + static_cast<std::size_t>(b)) * kStreamBufferFrames * 2;
            g_streams[s].buffers[b].data_pcm16 = g_stream_memory + at;
            g_streams[s].buffers[b].status = NDSP_WBUF_FREE;
        }
        g_streams[s].handed_out = -1;
    }
    g_ready = true;
    return true;
}

void audio_shutdown() {
    if (!g_ready) return;
    for (int c = 0; c < 24; ++c) ndspChnWaveBufClear(c);
    ndspExit();
    linearFree(g_stream_memory);
    g_stream_memory = nullptr;
    g_ready = false;
}

bool sample_create(std::uint32_t count, int channels, SampleInfo& out) {
    out = {};
    const std::uint32_t bytes = count * static_cast<std::uint32_t>(channels) * 2;
    auto* sample = static_cast<Sample3ds*>(std::malloc(sizeof(Sample3ds)));
    auto* frames = static_cast<std::int16_t*>(linearAlloc(bytes));
    if (sample == nullptr || frames == nullptr) {
        std::free(sample);
        if (frames != nullptr) linearFree(frames);
        return false;
    }
    *sample = Sample3ds{frames, bytes};
    out.handle = reinterpret_cast<SampleHandle>(sample);
    out.frames = frames;
    out.bytes = bytes;
    return true;
}

void sample_ready(SampleHandle handle) {
    const auto* sample = reinterpret_cast<const Sample3ds*>(handle);
    DSP_FlushDataCache(sample->frames, sample->bytes);
}

void sample_destroy(SampleHandle handle) {
    auto* sample = reinterpret_cast<Sample3ds*>(handle);
    linearFree(sample->frames);
    std::free(sample);
}

void voice_start(int voice, SampleHandle handle, std::uint32_t count, int channels, bool loop, const VoiceParams& params) {
    const auto* sample = reinterpret_cast<const Sample3ds*>(handle);
    setup_channel(voice, channels, params);
    ndspWaveBuf& b = g_voice_buffers[voice];
    std::memset(&b, 0, sizeof b);
    b.data_pcm16 = sample->frames;
    b.nsamples = count;
    b.looping = loop;
    ndspChnWaveBufAdd(voice, &b);
    g_voice_active[voice] = true;
}

void voice_update(int voice, const VoiceParams& params) { set_mix(voice, params); }

void voice_pause(int voice, bool paused) { ndspChnSetPaused(voice, paused); }

void voice_stop(int voice) {
    ndspChnWaveBufClear(voice);
    ndspChnSetPaused(voice, false);
    g_voice_buffers[voice].status = NDSP_WBUF_FREE;
    g_voice_active[voice] = false;
}

bool voice_playing(int voice) { return g_voice_active[voice] && queued(g_voice_buffers[voice]); }

void stream_start(int stream, int channels, const VoiceParams& params) {
    stream_stop(stream);
    g_streams[stream].channels = channels;
    setup_channel(kFirstStreamChannel + stream, channels, params);
}

std::int16_t* stream_buffer(int stream, std::uint32_t& capacity) {
    Stream3ds& s = g_streams[stream];
    for (int b = 0; b < kStreamBuffers; ++b) {
        if (!queued(s.buffers[b])) {
            s.handed_out = b;
            capacity = kStreamBufferFrames;
            return s.buffers[b].data_pcm16;
        }
    }
    capacity = 0;
    return nullptr;
}

void stream_submit(int stream, std::uint32_t frames) {
    Stream3ds& s = g_streams[stream];
    if (s.handed_out < 0) return;
    ndspWaveBuf& b = s.buffers[s.handed_out];
    s.handed_out = -1;
    b.nsamples = frames;
    b.looping = false;
    DSP_FlushDataCache(b.data_pcm16, frames * static_cast<std::uint32_t>(s.channels) * 2);
    ndspChnWaveBufAdd(kFirstStreamChannel + stream, &b);
}

void stream_update(int stream, const VoiceParams& params) { set_mix(kFirstStreamChannel + stream, params); }

void stream_pause(int stream, bool paused) { ndspChnSetPaused(kFirstStreamChannel + stream, paused); }

void stream_stop(int stream) {
    const int channel = kFirstStreamChannel + stream;
    ndspChnWaveBufClear(channel);
    ndspChnSetPaused(channel, false);
    for (ndspWaveBuf& b : g_streams[stream].buffers) b.status = NDSP_WBUF_FREE;
    g_streams[stream].handed_out = -1;
}

std::uint32_t stream_queued(int stream) {
    std::uint32_t total = 0;
    for (const ndspWaveBuf& b : g_streams[stream].buffers) {
        if (b.status == NDSP_WBUF_QUEUED) total += b.nsamples;
        if (b.status == NDSP_WBUF_PLAYING) {
            const std::uint32_t played = ndspChnGetSamplePos(kFirstStreamChannel + stream);
            total += b.nsamples > played ? b.nsamples - played : 0;
        }
    }
    return total;
}

} // namespace nnh::platform
