#include "audio_mixer_pc.hpp"

namespace nnh::platform::pc {

namespace {

constexpr float kScale = 1.0f / 32768.0f;

float clamp_unit(float v) { return v > 1.0f ? 1.0f : (v < -1.0f ? -1.0f : v); }

} // namespace

void PcMixer::reset() {
    for (Voice& v : voices_) v = Voice{};
    for (int i = 0; i < platform::kStreams; ++i) stream_stop(i);
}

void PcMixer::voice_start(int voice, const PcSample* sample, bool loop, const platform::VoiceParams& params) {
    Voice& v = voices_[voice];
    v = Voice{};
    v.sample = sample;
    v.playing = sample != nullptr && sample->count > 0;
    v.loop = loop;
    v.step = static_cast<double>(params.rate) / kOutputRate;
    v.gain = v.target = Gains{params.gain_left, params.gain_right};
}

void PcMixer::voice_update(int voice, const platform::VoiceParams& params) {
    Voice& v = voices_[voice];
    v.step = static_cast<double>(params.rate) / kOutputRate;
    v.target = Gains{params.gain_left, params.gain_right};
}

void PcMixer::voice_pause(int voice, bool paused) { voices_[voice].paused = paused; }

void PcMixer::voice_stop(int voice) { voices_[voice] = Voice{}; }

bool PcMixer::voice_playing(int voice) const { return voices_[voice].playing; }

bool PcMixer::sample_in_use(const PcSample* sample) const {
    for (const Voice& v : voices_) {
        if (v.playing && v.sample == sample) return true;
    }
    return false;
}

void PcMixer::mix_voice(Voice& v, float* out, std::uint32_t frames) {
    const PcSample& s = *v.sample;
    const auto count = static_cast<double>(s.count);
    for (std::uint32_t i = 0; i < frames && v.playing; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(frames);
        const float gl = v.gain.left + (v.target.left - v.gain.left) * t;
        const float gr = v.gain.right + (v.target.right - v.gain.right) * t;
        const auto a = static_cast<std::uint32_t>(v.position);
        std::uint32_t b = a + 1;
        if (b >= s.count) b = v.loop ? 0 : a;
        const float frac = static_cast<float>(v.position - a);
        const std::int16_t* fa = s.frames + static_cast<std::size_t>(a) * static_cast<std::size_t>(s.channels);
        const std::int16_t* fb = s.frames + static_cast<std::size_t>(b) * static_cast<std::size_t>(s.channels);
        const float left = (fa[0] + (fb[0] - fa[0]) * frac) * kScale;
        const float right = s.channels == 2 ? (fa[1] + (fb[1] - fa[1]) * frac) * kScale : left;
        out[2 * i] += left * gl;
        out[2 * i + 1] += right * gr;
        v.position += v.step;
        if (v.position >= count) {
            if (v.loop) {
                while (v.position >= count) v.position -= count;
            } else {
                v.playing = false;
            }
        }
    }
    v.gain = v.target;
}

void PcMixer::stream_start(int stream, int channels, const platform::VoiceParams& params) {
    stream_stop(stream);
    Stream& s = streams_[stream];
    s.active = true;
    s.channels = channels;
    s.step = static_cast<double>(params.rate) / kOutputRate;
    s.gain = s.target = Gains{params.gain_left, params.gain_right};
}

std::int16_t* PcMixer::stream_buffer(int stream, std::uint32_t& capacity) {
    Stream& s = streams_[stream];
    capacity = 0;
    if (!s.active) return nullptr;
    for (int b = 0; b < platform::kStreamBuffers; ++b) {
        bool queued = false;
        for (int q = 0; q < s.queued; ++q) queued = queued || s.order[q] == b;
        if (!queued) {
            s.handed_out = b;
            capacity = platform::kStreamBufferFrames;
            return s.data[b];
        }
    }
    return nullptr;
}

void PcMixer::stream_submit(int stream, std::uint32_t frames) {
    Stream& s = streams_[stream];
    if (s.handed_out < 0 || s.queued == platform::kStreamBuffers) return;
    s.frames[s.handed_out] = frames;
    s.order[s.queued++] = s.handed_out;
    s.handed_out = -1;
}

void PcMixer::stream_update(int stream, const platform::VoiceParams& params) {
    Stream& s = streams_[stream];
    s.step = static_cast<double>(params.rate) / kOutputRate;
    s.target = Gains{params.gain_left, params.gain_right};
}

void PcMixer::stream_pause(int stream, bool paused) { streams_[stream].paused = paused; }

void PcMixer::stream_stop(int stream) {
    Stream& s = streams_[stream];
    s.active = s.paused = false;
    s.queued = 0;
    s.handed_out = -1;
    s.position = 0;
}

std::uint32_t PcMixer::stream_queued(int stream) const {
    const Stream& s = streams_[stream];
    std::uint32_t total = 0;
    for (int q = 0; q < s.queued; ++q) total += s.frames[s.order[q]];
    const auto played = static_cast<std::uint32_t>(s.position);
    return total > played ? total - played : 0;
}

float PcMixer::stream_value(const Stream& s, std::uint32_t offset, int c) {
    for (int q = 0; q < s.queued; ++q) {
        const int b = s.order[q];
        if (offset < s.frames[b]) return s.data[b][static_cast<std::size_t>(offset) * static_cast<std::size_t>(s.channels) + static_cast<std::size_t>(c)] * kScale;
        offset -= s.frames[b];
    }
    return 0.0f;
}

void PcMixer::mix_stream(Stream& s, float* out, std::uint32_t frames) {
    for (std::uint32_t i = 0; i < frames && s.queued > 0; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(frames);
        const float gl = s.gain.left + (s.target.left - s.gain.left) * t;
        const float gr = s.gain.right + (s.target.right - s.gain.right) * t;
        const auto a = static_cast<std::uint32_t>(s.position);
        const float frac = static_cast<float>(s.position - a);
        const float l0 = stream_value(s, a, 0);
        const float l1 = frac > 0 ? stream_value(s, a + 1, 0) : l0;
        const float left = l0 + (l1 - l0) * frac;
        float right = left;
        if (s.channels == 2) {
            const float r0 = stream_value(s, a, 1);
            const float r1 = frac > 0 ? stream_value(s, a + 1, 1) : r0;
            right = r0 + (r1 - r0) * frac;
        }
        out[2 * i] += left * gl;
        out[2 * i + 1] += right * gr;
        s.position += s.step;
        // Drop the buffers fully played.
        while (s.queued > 0 && s.position >= s.frames[s.order[0]]) {
            s.position -= s.frames[s.order[0]];
            for (int q = 1; q < s.queued; ++q) s.order[q - 1] = s.order[q];
            --s.queued;
        }
        if (s.queued == 0) s.position = 0; // ran dry: wait for more
    }
    s.gain = s.target;
}

void PcMixer::mix(float* out, std::uint32_t frames) {
    for (std::uint32_t i = 0; i < 2 * frames; ++i) out[i] = 0.0f;
    for (Voice& v : voices_) {
        if (v.playing && !v.paused) mix_voice(v, out, frames);
    }
    for (Stream& s : streams_) {
        if (s.active && !s.paused) mix_stream(s, out, frames);
    }
    for (std::uint32_t i = 0; i < 2 * frames; ++i) out[i] = clamp_unit(out[i]);
}

} // namespace nnh::platform::pc
