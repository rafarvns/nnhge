// Fake platform: audio streams (contract nnh/platform/audio.hpp). Four buffers per stream, queued in
// order; tests "play" them with consume_stream() and read back everything submitted with stream_log().

#include <cstdlib>
#include <cstring>

#include "fake_internal.hpp"
#include "nnh/platform/audio.hpp"

namespace {

using nnh::platform::kStreamBufferFrames;
using nnh::platform::kStreamBuffers;
using nnh::platform::kStreams;
using nnh::platform_fake::detail::mutable_state;

constexpr std::uint32_t kLogValues = 1u << 20;

struct Buffers {
    std::int16_t data[kStreamBuffers][kStreamBufferFrames * 2];
    std::uint32_t frames[kStreamBuffers] = {}; // queued frames left in each buffer (0 = free)
    int order[kStreamBuffers] = {};            // queue: buffer indexes, oldest first
    int queued = 0;
    int handed_out = -1;                       // stream_buffer() result awaiting stream_submit()
    std::int16_t log[kLogValues];
    std::uint32_t logged_values = 0;
};

Buffers g_buffers[kStreams];

nnh::platform_fake::FakeStream& stream_at(int stream) {
    if (stream < 0 || stream >= kStreams) std::abort(); // core broke the contract
    return mutable_state().streams[stream];
}

void clear(int stream) {
    Buffers& b = g_buffers[stream];
    for (std::uint32_t& f : b.frames) f = 0;
    b.queued = 0;
    b.handed_out = -1;
}

} // namespace

namespace nnh::platform_fake {

void consume_stream(int stream, std::uint32_t frames) {
    FakeStream& s = stream_at(stream);
    Buffers& b = g_buffers[stream];
    while (frames > 0 && b.queued > 0) {
        const int front = b.order[0];
        const std::uint32_t n = frames < b.frames[front] ? frames : b.frames[front];
        b.frames[front] -= n;
        s.queued -= n;
        frames -= n;
        if (b.frames[front] == 0) {
            for (int i = 1; i < b.queued; ++i) b.order[i - 1] = b.order[i];
            --b.queued;
        }
    }
}

const std::int16_t* stream_log(int stream, std::uint32_t& frames) {
    const FakeStream& s = stream_at(stream);
    frames = s.channels > 0 ? g_buffers[stream].logged_values / static_cast<std::uint32_t>(s.channels) : 0;
    return g_buffers[stream].log;
}

} // namespace nnh::platform_fake

namespace nnh::platform {

void stream_start(int stream, int channels, const VoiceParams& params) {
    if (channels < 1 || channels > 2) std::abort();
    platform_fake::FakeStream& s = stream_at(stream);
    clear(stream);
    g_buffers[stream].logged_values = 0;
    const int starts = s.starts + 1;
    const int stops = s.stops;
    s = {};
    s.active = true;
    s.channels = channels;
    s.params = params;
    s.starts = starts;
    s.stops = stops;
}

std::int16_t* stream_buffer(int stream, std::uint32_t& capacity) {
    if (!stream_at(stream).active) std::abort();
    Buffers& b = g_buffers[stream];
    for (int i = 0; i < kStreamBuffers; ++i) {
        bool in_queue = false;
        for (int q = 0; q < b.queued; ++q) in_queue = in_queue || b.order[q] == i;
        if (!in_queue) {
            b.handed_out = i;
            capacity = kStreamBufferFrames;
            return b.data[i];
        }
    }
    capacity = 0;
    return nullptr;
}

void stream_submit(int stream, std::uint32_t frames) {
    platform_fake::FakeStream& s = stream_at(stream);
    Buffers& b = g_buffers[stream];
    if (b.handed_out < 0 || frames == 0 || frames > kStreamBufferFrames) std::abort();
    const int i = b.handed_out;
    b.handed_out = -1;
    b.frames[i] = frames;
    b.order[b.queued++] = i;
    s.queued += frames;
    s.submitted += frames;
    const std::uint32_t values = frames * static_cast<std::uint32_t>(s.channels);
    const std::uint32_t room = kLogValues - b.logged_values;
    const std::uint32_t copy = values < room ? values : room;
    std::memcpy(b.log + b.logged_values, b.data[i], copy * sizeof(std::int16_t));
    b.logged_values += copy;
}

void stream_update(int stream, const VoiceParams& params) { stream_at(stream).params = params; }

void stream_pause(int stream, bool paused) { stream_at(stream).paused = paused; }

void stream_stop(int stream) {
    platform_fake::FakeStream& s = stream_at(stream);
    clear(stream);
    s.active = false;
    s.paused = false;
    s.queued = 0;
    ++s.stops;
}

std::uint32_t stream_queued(int stream) { return stream_at(stream).queued; }

} // namespace nnh::platform
