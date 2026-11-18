// Music (nnh::music): two track slots (a crossfade plays both), each streaming an asset into a platform
// stream. Every frame, core tops up the stream's free buffers; a track that doesn't loop ends when the
// platform has played all it was given. Without audio output, time is kept by the clock.

#include "assets/assets_internal.hpp"
#include "audio/audio_internal.hpp"
#include "audio/fade.hpp"
#include "audio/music_track.hpp"
#include "log/engine_channels.hpp"
#include "nnh/assets.hpp"
#include "nnh/events.hpp"
#include "nnh/log.hpp"
#include "nnh/platform/assets.hpp"

namespace nnh {

namespace {

using detail::audio_state;

struct Slot {
    bool active = false;
    bool current = false;   ///< the track music:: talks about (not one fading out)
    bool stopping = false;  ///< fading out, closed when the fade ends
    bool finishing = false; ///< everything was decoded; ends when the platform played it
    platform::AssetStream asset = 0;
    std::uint64_t asset_size = 0;
    void* arena = nullptr;
    float volume = 1.0f;
    detail::Fade fade;
    std::uint64_t submitted = 0;  ///< frames given to the platform since the start
    double silent_played = 0;     ///< frames "played" without audio output
    detail::MusicTrack track;     ///< ~21 KB: the slots are static
};

Slot g_slots[platform::kStreams];
bool g_paused = false;

bool output() { return audio_state().available; }
bool effective_paused() { return g_paused || audio_state().mixer.group_paused(AudioGroup::Music); }

std::size_t read_asset(void* context, std::uint64_t offset, void* out, std::size_t n) {
    std::size_t read = 0;
    return platform::asset_read_at(static_cast<Slot*>(context)->asset, offset, out, n, read) ? read : 0;
}

void send_params(int i) {
    const Slot& s = g_slots[i];
    if (!output()) return;
    platform::stream_update(i, audio_state().mixer.params(s.volume * s.fade.value(), 0.0f, 1.0f, s.track.rate(), AudioGroup::Music));
}

void send_pause(int i) {
    if (output()) platform::stream_pause(i, effective_paused());
}

void close_slot(int i) {
    Slot& s = g_slots[i];
    if (!s.active && s.asset == 0 && s.arena == nullptr) return;
    if (s.active && output()) platform::stream_stop(i);
    s.track.close();
    if (s.asset != 0) platform::asset_close(s.asset);
    mem::free(s.arena);
    s.active = s.current = s.stopping = s.finishing = false;
    s.asset = 0;
    s.arena = nullptr;
    s.submitted = 0;
    s.silent_played = 0;
}

void fill(int i) {
    Slot& s = g_slots[i];
    while (output() && !s.finishing) {
        std::uint32_t capacity = 0;
        std::int16_t* buffer = platform::stream_buffer(i, capacity);
        if (buffer == nullptr) return;
        auto got = s.track.read(buffer, capacity);
        if (!got) {
            NNH_LOG_WARN(audio, "music stopped early: %s", got.error().message);
            s.finishing = true;
            return;
        }
        if (got.value() > 0) {
            platform::stream_submit(i, got.value());
            s.submitted += got.value();
        }
        if (got.value() < capacity) s.finishing = true;
    }
}

/// Opens `path` in slot `i` and starts its stream (fading in over `fade_in`).
Result<void> open_slot(int i, const char* path, const music::Options& options, float fade_in) {
    char asset[detail::kMaxAssetPath + 1];
    auto found = detail::find_audio_asset(path, asset);
    if (!found) return found.error();
    const bool ogg = found.value();
    Slot& s = g_slots[i];
    auto size = assets::size(asset);
    if (!size || !platform::asset_open(asset, s.asset)) return Error::make(ErrorCode::IoError, "\"%s\": can't open", path);
    s.asset_size = size.value();
    if (ogg) {
        s.arena = mem::alloc(MemRegion::Heap, detail::audio_memory_tag(), detail::OggDecoder::kDefaultArenaBytes, 16);
        if (s.arena == nullptr) {
            close_slot(i);
            return Error::make(ErrorCode::OutOfMemory, "\"%s\": no memory for the Ogg decoder", path);
        }
    }
    const detail::ByteSource source{&s, s.asset_size, read_asset};
    auto opened = s.track.open(source, ogg, s.arena, detail::OggDecoder::kDefaultArenaBytes, options.loop_start, options.loop);
    if (!opened) {
        NNH_LOG_WARN(audio, "music \"%s\": %s", path, opened.error().message);
        close_slot(i);
        return opened.error();
    }
    s.active = true;
    s.volume = detail::clamp_volume(options.volume);
    s.fade = fade_in > 0 ? detail::Fade::between(0.0f, 1.0f, fade_in) : detail::Fade{};
    if (output()) {
        platform::stream_start(i, s.track.channels(),
                               audio_state().mixer.params(s.volume * s.fade.value(), 0.0f, 1.0f, s.track.rate(), AudioGroup::Music));
        if (effective_paused()) platform::stream_pause(i, true);
        fill(i);
    }
    return {};
}

int current_slot() {
    for (int i = 0; i < platform::kStreams; ++i) {
        if (g_slots[i].active && g_slots[i].current) return i;
    }
    return -1;
}

void register_music();

/// Opens a new current track in the slot that isn't current, replacing a track fading out there.
Result<int> open_new(const char* path, const music::Options& options, float fade_in) {
    if (!audio_state().ready) return Error::make(ErrorCode::Unsupported, "music before nnh::init()");
    if (path == nullptr) return Error::make(ErrorCode::InvalidArgument, "music path is null");
    register_music(); // from now on the frame loop runs music
    const int target = current_slot() == 0 ? 1 : 0;
    close_slot(target);
    auto opened = open_slot(target, path, options, fade_in);
    if (!opened) return opened.error();
    return target;
}

void end_track(int i) {
    const bool was_current = g_slots[i].current;
    close_slot(i);
    if (was_current) events::publish(MusicFinished{});
}

} // namespace

namespace {

void music_shutdown() {
    for (int i = 0; i < platform::kStreams; ++i) close_slot(i);
    g_paused = false;
}

void music_mix_changed() {
    for (int i = 0; i < platform::kStreams; ++i) {
        if (!g_slots[i].active) continue;
        send_params(i);
        send_pause(i);
    }
}

void music_frame(Duration delta) {
    const double seconds = delta.seconds();
    for (int i = 0; i < platform::kStreams; ++i) {
        Slot& s = g_slots[i];
        if (!s.active) continue;
        s.fade.advance(seconds);
        if (s.stopping && s.fade.done()) {
            close_slot(i);
            continue;
        }
        send_params(i);
        if (output()) {
            fill(i);
            if (s.finishing && platform::stream_queued(i) == 0) end_track(i);
        } else if (!effective_paused()) {
            s.silent_played += seconds * s.track.rate();
            if (!s.track.loops() && s.silent_played >= static_cast<double>(s.track.length())) end_track(i);
        }
    }
}

constexpr detail::MusicHooks kHooks{music_frame, music_shutdown, music_mix_changed};

void register_music() { audio_state().music = &kHooks; }

} // namespace

namespace music {

Result<void> play(const char* path, const Options& options) {
    const int previous = current_slot();
    auto opened = open_new(path, options, options.fade_in);
    if (!opened) return opened.error();
    if (previous >= 0) close_slot(previous);
    g_slots[opened.value()].current = true;
    return {};
}

Result<void> crossfade(const char* path, float seconds, const Options& options) {
    const int previous = current_slot();
    auto opened = open_new(path, options, seconds);
    if (!opened) return opened.error();
    if (previous >= 0) {
        Slot& old = g_slots[previous];
        old.current = false;
        old.stopping = true;
        old.fade = detail::Fade::between(old.fade.value(), 0.0f, seconds);
    }
    g_slots[opened.value()].current = true;
    return {};
}

void stop(float fade_out) {
    const int i = current_slot();
    if (i < 0) return;
    Slot& s = g_slots[i];
    if (fade_out <= 0) {
        close_slot(i);
        return;
    }
    s.current = false;
    s.stopping = true;
    s.fade = detail::Fade::between(s.fade.value(), 0.0f, fade_out);
}

void set_paused(bool paused) {
    g_paused = paused;
    for (int i = 0; i < platform::kStreams; ++i) {
        if (g_slots[i].active) send_pause(i);
    }
}

bool paused() { return g_paused; }

void set_volume(float volume) {
    const int i = current_slot();
    if (i < 0) return;
    g_slots[i].volume = detail::clamp_volume(volume);
    send_params(i);
}

bool playing() { return current_slot() >= 0; }

float position() {
    const int i = current_slot();
    if (i < 0) return 0.0f;
    const Slot& s = g_slots[i];
    const std::uint64_t played = output() ? s.submitted - platform::stream_queued(i)
                                          : static_cast<std::uint64_t>(s.silent_played);
    return static_cast<float>(static_cast<double>(s.track.position_at(played)) / s.track.rate());
}

} // namespace music
} // namespace nnh
