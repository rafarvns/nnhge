// Music tracks (audio/music_track.hpp, audio/wav_stream.hpp, audio/fade.hpp; spec 008 FR-3): a WAV read
// in pieces, reading across the loop point, the loop point rules, positions on the timeline, fades.

#include <cstring>

#include "audio/fade.hpp"
#include "audio/music_track.hpp"
#include "audio/wav_stream.hpp"
#include "audio_test_util.hpp"
#include "test.hpp"

using nnh::ErrorCode;
using nnh::detail::ByteSource;
using nnh::detail::Fade;
using nnh::detail::MusicTrack;
using nnh::detail::WavStream;

namespace {

constexpr std::uint32_t kFrames = 10000;
std::uint8_t g_file[64 + kFrames * 4];
std::size_t g_size = 0;
std::uint8_t g_ogg[16 * 1024];
std::size_t g_ogg_size = 0;
alignas(16) unsigned char g_arena[nnh::detail::OggDecoder::kDefaultArenaBytes];

struct Mem {
    const std::uint8_t* data;
    std::size_t size;
};
Mem g_mem;
Mem g_ogg_mem;

std::size_t read_mem(void* c, std::uint64_t offset, void* out, std::size_t n) {
    const auto* m = static_cast<const Mem*>(c);
    if (offset >= m->size) return 0;
    const std::size_t k = n < m->size - offset ? n : static_cast<std::size_t>(m->size - offset);
    std::memcpy(out, m->data + offset, k);
    return k;
}

/// A stereo 16-bit WAV whose frame i holds (i, -i) (mod 2^15).
ByteSource stereo_wav() {
    g_size = audio_test::make_wav(g_file, 2, 16, 22050, kFrames, [](std::uint32_t v) {
        const int frame = static_cast<int>(v / 2) % 32768;
        return v % 2 == 0 ? frame : -frame;
    });
    g_mem = {g_file, g_size};
    return {&g_mem, g_size, read_mem};
}

ByteSource ogg_loop() {
    g_ogg_size = audio_test::load_fixture("sweep_stereo_32k_loop.ogg", g_ogg, sizeof g_ogg);
    g_ogg_mem = {g_ogg, g_ogg_size};
    return {&g_ogg_mem, g_ogg_size, read_mem};
}

std::int16_t g_out[2 * 30000];

} // namespace

NNH_TEST(wav_stream_reads_any_piece_and_seeks) {
    static WavStream wav;
    CHECK(wav.open(stereo_wav()).ok());
    CHECK(wav.channels() == 2 && wav.rate() == 22050 && wav.length() == kFrames);
    auto a = wav.read(g_out, 3);
    CHECK(a.ok() && a.value() == 3 && g_out[4] == 2 && g_out[5] == -2);
    CHECK(wav.seek(9998).ok());
    auto b = wav.read(g_out, 100); // only 2 left
    CHECK(b.ok() && b.value() == 2 && g_out[0] == 9998 && g_out[3] == -9999);
    auto end = wav.read(g_out, 10);
    CHECK(end.ok() && end.value() == 0);
    CHECK(!wav.seek(kFrames + 1).ok());
    CHECK(wav.seek(1234).ok() && wav.read(g_out, 1).value() == 1 && g_out[0] == 1234);
}

NNH_TEST(wav_stream_reads_large_pieces_in_several_reads) {
    static WavStream wav;
    CHECK(wav.open(stereo_wav()).ok());
    auto r = wav.read(g_out, kFrames); // 40 KB: more than its read buffer
    CHECK(r.ok() && r.value() == kFrames);
    bool same = true;
    for (std::uint32_t i = 0; i < kFrames; ++i) same = same && g_out[2 * i] == static_cast<std::int16_t>(i);
    CHECK(same);
}

NNH_TEST(wav_stream_rejects_what_it_cannot_play) {
    static WavStream wav;
    static const std::uint8_t junk[] = "RIFF....WAVEjunkjunk";
    Mem m{junk, sizeof junk};
    CHECK(wav.open({&m, sizeof junk, read_mem}).error().code == ErrorCode::Corrupted);
}

NNH_TEST(track_without_loop_stops_at_the_end) {
    static MusicTrack track;
    CHECK(track.open(stereo_wav(), false, nullptr, 0, 0, false).ok());
    auto r = track.read(g_out, 30000);
    CHECK(r.ok() && r.value() == kFrames);
    CHECK(track.read(g_out, 10).value() == 0);
    CHECK(track.position_at(kFrames + 500) == kFrames); // stays at the end
}

NNH_TEST(track_loops_back_to_the_loop_point_inside_one_read) {
    static MusicTrack track;
    CHECK(track.open(stereo_wav(), false, nullptr, 0, 4000, true).ok());
    CHECK(track.loop_start() == 4000);
    auto r = track.read(g_out, 13000); // 10000, then 3000 from frame 4000
    CHECK(r.ok() && r.value() == 13000);
    CHECK(g_out[2 * 9999] == 9999 && g_out[2 * 10000] == 4000 && g_out[2 * 12999] == 6999);
    // Timeline (frames played since the start) -> position in the track.
    CHECK(track.position_at(9999) == 9999);
    CHECK(track.position_at(10000) == 4000);
    CHECK(track.position_at(10000 + 6000) == 4000); // one full loop later
    CHECK(track.position_at(10000 + 6001) == 4001);
}

NNH_TEST(track_loop_point_must_lie_inside_the_track) {
    static MusicTrack track;
    CHECK(track.open(stereo_wav(), false, nullptr, 0, kFrames, true).error().code == ErrorCode::InvalidArgument);
    CHECK(track.open(stereo_wav(), false, nullptr, 0, 0, true).ok());
    CHECK(track.loop_start() == 0);
}

NNH_TEST(track_uses_the_ogg_loopstart_tag_unless_given_one) {
    static MusicTrack track;
    CHECK(track.open(ogg_loop(), true, g_arena, sizeof g_arena, 0, true).ok());
    CHECK(track.channels() == 2 && track.rate() == 32000 && track.loop_start() == 12345);
    track.close();
    CHECK(track.open(ogg_loop(), true, g_arena, sizeof g_arena, 100, true).ok());
    CHECK(track.loop_start() == 100);
    auto r = track.read(g_out, 30000); // 30000 < 32000: no wrap yet
    CHECK(r.ok() && r.value() == 30000);
    track.close();
}

NNH_TEST(fade_moves_linearly_and_holds_its_target) {
    Fade still;
    CHECK(still.value() == 1.0f && still.done());
    Fade f = Fade::between(0.0f, 1.0f, 2.0);
    CHECK(f.value() == 0.0f && !f.done());
    f.advance(0.5);
    CHECK(f.value() > 0.249f && f.value() < 0.251f);
    f.advance(5.0);
    CHECK(f.value() == 1.0f && f.done());
    Fade instant = Fade::between(1.0f, 0.0f, 0.0);
    CHECK(instant.value() == 0.0f && instant.done());
}
