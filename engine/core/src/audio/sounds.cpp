// Sounds (nnh::Sound, nnh::load_sound): a WAV or Ogg Vorbis asset decoded whole into platform memory.

#include <cstdio>
#include <cstring>
#include <utility>

#include "assets/asset_path.hpp"
#include "assets/assets_internal.hpp"
#include "audio/audio_internal.hpp"
#include "audio/sound_decode.hpp"
#include "log/engine_channels.hpp"
#include "memory/memory_internal.hpp"
#include "nnh/assets.hpp"
#include "nnh/log.hpp"

namespace nnh {

namespace {

using detail::audio_state;
using detail::SoundSlot;

std::uint32_t g_next_generation = 1; // never reused, so handles from an earlier init() stay stale

Error refuse(ErrorCode code, const char* path, const char* what) {
    NNH_LOG_WARN(audio, "load_sound(\"%s\"): %s", path, what);
    return Error::make(code, "sound \"%s\": %s", path, what);
}

int free_slot() {
    detail::AudioState& a = audio_state();
    for (int i = 0; i < a.max_sounds; ++i) {
        if (!a.sounds[i].active) return i;
    }
    return -1;
}

/// The asset of `path`: path.wav, else path.ogg. Writes the full name to `out`.
Result<bool> find_sound_asset(const char* path, char (&out)[detail::kMaxAssetPath + 1]) {
    for (const char* ext : {".wav", ".ogg"}) {
        const int n = std::snprintf(out, sizeof out, "%s%s", path, ext);
        if (n < 0 || n >= static_cast<int>(sizeof out) || !detail::valid_asset_path(out)) {
            return refuse(ErrorCode::InvalidArgument, path, "invalid asset path");
        }
        if (detail::asset_lookup(out) != nullptr) return std::strcmp(ext, ".ogg") == 0;
    }
    return refuse(ErrorCode::NotFound, path, "no .wav or .ogg with that path in the game's assets");
}

struct MemoryBytes {
    const std::uint8_t* data;
    std::size_t size;
};

std::size_t read_memory(void* context, std::uint64_t offset, void* out, std::size_t n) {
    const auto* m = static_cast<const MemoryBytes*>(context);
    if (offset >= m->size) return 0;
    const std::size_t count = n < m->size - offset ? n : static_cast<std::size_t>(m->size - offset);
    std::memcpy(out, m->data + offset, count);
    return count;
}

/// Decodes `file` into a new slot (and platform sample when there is audio output).
Result<int> decode_into_slot(const char* path, const AssetData& file, bool ogg) {
    static detail::OggDecoder decoder; // ~17 KB: static, not on the (small 3DS) stack
    const auto* bytes = static_cast<const std::uint8_t*>(file.data());
    detail::WavFormat wav;
    void* arena = nullptr;
    SoundSlot slot;
    if (!ogg) {
        auto parsed = detail::parse_wav(bytes, file.size(), file.size());
        if (!parsed) return refuse(parsed.error().code, path, parsed.error().message);
        wav = parsed.value();
        slot = {true, 0, 0, wav.frames(), wav.channels, wav.rate};
    } else {
        arena = mem::alloc(MemRegion::Heap, detail::audio_memory_tag(), detail::OggDecoder::kDefaultArenaBytes, 16);
        if (arena == nullptr) return refuse(ErrorCode::OutOfMemory, path, "no memory to decode Ogg Vorbis");
        MemoryBytes source{bytes, file.size()};
        auto opened = decoder.open({&source, file.size(), read_memory}, arena, detail::OggDecoder::kDefaultArenaBytes);
        if (!opened) {
            mem::free(arena);
            return refuse(opened.error().code, path, opened.error().message);
        }
        slot = {true, 0, 0, static_cast<std::uint32_t>(decoder.length()), decoder.channels(), decoder.rate()};
    }
    auto finish = [&](Result<int> result) {
        decoder.close();
        mem::free(arena);
        return result;
    };
    const std::size_t pcm_bytes = static_cast<std::size_t>(slot.frames) * static_cast<std::size_t>(slot.channels) * 2;
    if (slot.frames == 0 || pcm_bytes > detail::kMaxSoundBytes) {
        return finish(refuse(ErrorCode::LimitReached, path, "empty or over 4 MB decoded: play it as music"));
    }
    const int index = free_slot();
    if (index < 0) return finish(refuse(ErrorCode::LimitReached, path, "too many sounds (AppConfig::audio.max_sounds)"));
    platform::SampleInfo info;
    if (audio_state().available && !platform::sample_create(slot.frames, slot.channels, info)) {
        return finish(refuse(ErrorCode::OutOfMemory, path, "no audio memory left"));
    }
    if (!ogg) {
        if (info.frames != nullptr) detail::wav_to_pcm16(bytes, wav, info.frames);
    } else if (auto decoded = detail::ogg_decode_all(decoder, info.frames, slot.frames); !decoded) {
        if (info.handle != 0) platform::sample_destroy(info.handle);
        return finish(refuse(decoded.error().code, path, decoded.error().message));
    }
    if (info.handle != 0) {
        platform::sample_ready(info.handle);
        detail::mem_account(MemRegion::Linear, detail::audio_memory_tag(), info.bytes, true);
    }
    slot.sample = info.handle;
    slot.bytes = info.bytes;
    slot.generation = g_next_generation++;
    audio_state().sounds[index] = slot;
    return finish(index);
}

} // namespace

detail::SoundSlot* detail::sound_get(int slot, std::uint32_t generation) {
    AudioState& a = audio_state();
    if (!a.ready || slot < 0 || slot >= a.max_sounds) return nullptr;
    SoundSlot& s = a.sounds[slot];
    return s.active && s.generation == generation ? &s : nullptr;
}

platform::SampleHandle detail::sound_sample(const Sound& sound) {
    const SoundSlot* s = sound_get(SoundAccess::slot(sound), SoundAccess::generation(sound));
    return s != nullptr ? s->sample : 0;
}

Sound::Sound(Sound&& other) noexcept : slot_(other.slot_), generation_(other.generation_) {
    other.slot_ = -1;
    other.generation_ = 0;
}

Sound& Sound::operator=(Sound&& other) noexcept {
    if (this != &other) {
        reset();
        slot_ = other.slot_;
        generation_ = other.generation_;
        other.slot_ = -1;
        other.generation_ = 0;
    }
    return *this;
}

bool Sound::valid() const { return detail::sound_get(slot_, generation_) != nullptr; }

float Sound::duration() const {
    const SoundSlot* s = detail::sound_get(slot_, generation_);
    return s != nullptr ? static_cast<float>(s->frames) / static_cast<float>(s->rate) : 0.0f;
}

void Sound::reset() {
    if (SoundSlot* s = detail::sound_get(slot_, generation_)) {
        detail::stop_voices_of(slot_);
        if (s->sample != 0) platform::sample_destroy(s->sample);
        if (s->bytes > 0) detail::mem_account(MemRegion::Linear, detail::audio_memory_tag(), s->bytes, false);
        *s = SoundSlot{};
    }
    slot_ = -1;
    generation_ = 0;
}

Result<Sound> load_sound(const char* path) {
    if (!audio_state().ready) return Error::make(ErrorCode::Unsupported, "load_sound before nnh::init()");
    if (path == nullptr) return Error::make(ErrorCode::InvalidArgument, "load_sound(nullptr)");
    char asset[detail::kMaxAssetPath + 1];
    auto found = find_sound_asset(path, asset);
    if (!found) return found.error();
    const bool ogg = found.value();
    if (!ogg) { // a WAV's samples are its size: refuse a huge one before reading it
        auto size = assets::size(asset);
        if (size && size.value() > detail::kMaxSoundBytes + 64 * 1024) {
            return refuse(ErrorCode::LimitReached, path, "over 4 MB decoded: play it as music");
        }
    }
    auto file = assets::load(asset);
    if (!file) return file.error();
    auto slot = decode_into_slot(path, file.value(), ogg);
    if (!slot) return slot.error();
    return SoundAccess::make(slot.value(), audio_state().sounds[slot.value()].generation);
}

} // namespace nnh
