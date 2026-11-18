// Fake platform: audio output that plays nothing and records every voice (contract nnh/platform/audio.hpp).

#include <cstdlib>
#include <new>

#include "fake_internal.hpp"
#include "nnh/platform/audio.hpp"

namespace {

using nnh::platform_fake::detail::controls;
using nnh::platform_fake::detail::mutable_state;

/// A fake sample: a header followed by its frames, from malloc (tests only).
struct FakeSample {
    std::uint32_t magic;
    std::uint32_t values; // count × channels
    std::int16_t frames[1];
};
constexpr std::uint32_t kMagic = 0x534D504Cu; // "SMPL"

FakeSample* sample_of(nnh::platform::SampleHandle handle) {
    auto* sample = reinterpret_cast<FakeSample*>(handle);
    return sample != nullptr && sample->magic == kMagic ? sample : nullptr;
}

nnh::platform_fake::FakeVoice* voice_at(int voice) {
    if (voice < 0 || voice >= nnh::platform_fake::kMaxFakeVoices) std::abort(); // core broke the contract
    return &mutable_state().voices[voice];
}

} // namespace

namespace nnh::platform_fake {

void set_audio_available(bool available) { controls().audio_unavailable = !available; }

void finish_voice(int voice) { voice_at(voice)->playing = false; }

const std::int16_t* sample_frames(platform::SampleHandle sample) {
    FakeSample* s = sample_of(sample);
    return s != nullptr ? s->frames : nullptr;
}

} // namespace nnh::platform_fake

namespace nnh::platform {

bool audio_init(int voices) {
    if (controls().audio_unavailable) {
        platform_fake::detail::set_last_error("fake audio unavailable (set_audio_available)");
        return false;
    }
    if (voices < 1 || voices > platform_fake::kMaxFakeVoices) std::abort();
    mutable_state().audio_initialized = true;
    mutable_state().audio_voices = voices;
    return true;
}

void audio_shutdown() {
    mutable_state().audio_initialized = false;
    for (auto& v : mutable_state().voices) v.playing = false;
}

bool sample_create(std::uint32_t count, int channels, SampleInfo& out) {
    out = {};
    const std::size_t values = static_cast<std::size_t>(count) * static_cast<std::size_t>(channels);
    void* memory = std::malloc(sizeof(FakeSample) + values * sizeof(std::int16_t));
    if (memory == nullptr) return false;
    auto* sample = new (memory) FakeSample{kMagic, static_cast<std::uint32_t>(values), {0}};
    out.handle = reinterpret_cast<SampleHandle>(sample);
    out.frames = sample->frames;
    out.bytes = values * sizeof(std::int16_t);
    ++mutable_state().live_samples;
    return true;
}

void sample_ready(SampleHandle sample) {
    if (sample_of(sample) == nullptr) std::abort();
    ++mutable_state().samples_ready;
}

void sample_destroy(SampleHandle sample) {
    FakeSample* s = sample_of(sample);
    if (s == nullptr) std::abort(); // destroyed twice or never created
    for (const auto& v : mutable_state().voices) {
        if (v.playing && v.sample == sample) std::abort(); // core must stop its voices first
    }
    s->magic = 0;
    std::free(s);
    --mutable_state().live_samples;
}

void voice_start(int voice, SampleHandle sample, std::uint32_t count, int channels, bool loop, const VoiceParams& params) {
    if (sample_of(sample) == nullptr) std::abort();
    platform_fake::FakeVoice* v = voice_at(voice);
    v->playing = true;
    v->paused = false;
    v->loop = loop;
    v->sample = sample;
    v->count = count;
    v->channels = channels;
    v->params = params;
    ++v->starts;
}

void voice_update(int voice, const VoiceParams& params) { voice_at(voice)->params = params; }

void voice_pause(int voice, bool paused) { voice_at(voice)->paused = paused; }

void voice_stop(int voice) {
    platform_fake::FakeVoice* v = voice_at(voice);
    v->playing = false;
    v->paused = false;
    ++v->stops;
}

bool voice_playing(int voice) { return voice_at(voice)->playing; }

} // namespace nnh::platform
