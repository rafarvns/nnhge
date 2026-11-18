// The WAV reader (audio/wav_reader.hpp, spec 008 FR-1, FR-10): PCM 8/16-bit, mono or stereo, 8–48 kHz;
// unknown chunks skipped; every size and offset checked against the bytes and the file.

#include <cstdint>
#include <cstring>

#include "audio/wav_reader.hpp"
#include "test.hpp"

using nnh::ErrorCode;
using nnh::detail::parse_wav;

namespace {

/// Builds a WAV file in memory, chunk by chunk.
struct WavBuilder {
    std::uint8_t bytes[512] = {};
    std::size_t size = 0;

    void raw(const void* data, std::size_t n) {
        std::memcpy(bytes + size, data, n);
        size += n;
    }
    void u16(std::uint32_t v) {
        const std::uint8_t b[2] = {std::uint8_t(v), std::uint8_t(v >> 8)};
        raw(b, 2);
    }
    void u32(std::uint32_t v) {
        const std::uint8_t b[4] = {std::uint8_t(v), std::uint8_t(v >> 8), std::uint8_t(v >> 16), std::uint8_t(v >> 24)};
        raw(b, 4);
    }
    void header() {
        raw("RIFF", 4);
        u32(0); // patched by finish()
        raw("WAVE", 4);
    }
    void fmt(std::uint32_t format, std::uint32_t channels, std::uint32_t rate, std::uint32_t bits) {
        raw("fmt ", 4);
        u32(16);
        u16(format);
        u16(channels);
        u32(rate);
        u32(rate * channels * bits / 8);
        u16(channels * bits / 8);
        u16(bits);
    }
    void chunk(const char* id, std::uint32_t declared, std::size_t actual) {
        raw(id, 4);
        u32(declared);
        size += actual; // zeros
    }
    void finish() {
        const std::uint32_t riff = static_cast<std::uint32_t>(size - 8);
        std::memcpy(bytes + 4, &riff, 4); // little-endian hosts only: PC and 3DS
    }
};

/// A valid 16-bit stereo 32 kHz file with `frames` frames of silence.
WavBuilder stereo16(std::uint32_t frames) {
    WavBuilder w;
    w.header();
    w.fmt(1, 2, 32000, 16);
    w.chunk("data", frames * 4, frames * 4);
    w.finish();
    return w;
}

} // namespace

NNH_TEST(wav_reads_the_format_and_where_the_samples_are) {
    WavBuilder w = stereo16(10);
    auto r = parse_wav(w.bytes, w.size, w.size);
    CHECK(r.ok());
    CHECK(r.value().channels == 2 && r.value().bits == 16 && r.value().rate == 32000);
    CHECK(r.value().data_offset == 44);
    CHECK(r.value().data_size == 40);
    CHECK(r.value().frames() == 10);
}

NNH_TEST(wav_accepts_8_bit_mono_and_the_rate_limits) {
    for (std::uint32_t rate : {8000u, 22050u, 48000u}) {
        WavBuilder w;
        w.header();
        w.fmt(1, 1, rate, 8);
        w.chunk("data", 5, 5);
        w.finish();
        auto r = parse_wav(w.bytes, w.size, w.size);
        CHECK(r.ok() && r.value().bits == 8 && r.value().channels == 1 && r.value().frames() == 5);
        CHECK(r.ok() && r.value().rate == rate);
    }
}

NNH_TEST(wav_skips_unknown_chunks_with_padding) {
    WavBuilder w;
    w.header();
    w.chunk("LIST", 3, 4); // odd size: padded to 4
    w.fmt(1, 1, 16000, 16);
    w.chunk("cue ", 8, 8);
    w.chunk("data", 6, 6);
    w.finish();
    auto r = parse_wav(w.bytes, w.size, w.size);
    CHECK(r.ok());
    CHECK(r.value().data_offset == 12 + 12 + 24 + 16 + 8);
    CHECK(r.value().frames() == 3);
}

NNH_TEST(wav_extensible_format_with_pcm_is_accepted) {
    WavBuilder w;
    w.header();
    w.raw("fmt ", 4);
    w.u32(40);
    w.u16(0xFFFE);
    w.u16(2);
    w.u32(44100);
    w.u32(44100 * 4);
    w.u16(4);
    w.u16(16);
    w.u16(22);    // extension size
    w.u16(16);    // valid bits
    w.u32(3);     // channel mask
    w.u16(1);     // sub-format: PCM GUID starts with 0x0001
    const std::uint8_t guid_tail[14] = {0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};
    w.raw(guid_tail, 14);
    w.chunk("data", 8, 8);
    w.finish();
    auto r = parse_wav(w.bytes, w.size, w.size);
    CHECK(r.ok() && r.value().channels == 2 && r.value().frames() == 2);
}

NNH_TEST(wav_with_only_the_header_in_memory_checks_the_data_against_the_file) {
    // Music streaming reads just the start of the file: the data may lie beyond the bytes given.
    WavBuilder w = stereo16(1000);
    auto r = parse_wav(w.bytes, 44, w.size);
    CHECK(r.ok() && r.value().data_offset == 44 && r.value().frames() == 1000);
    // ...but not beyond the end of the file.
    auto truncated = parse_wav(w.bytes, 44, w.size - 4);
    CHECK(!truncated.ok() && truncated.error().code == ErrorCode::Corrupted);
}

NNH_TEST(wav_drops_a_partial_last_frame) {
    WavBuilder w;
    w.header();
    w.fmt(1, 2, 32000, 16);
    w.chunk("data", 10, 10); // 2 frames of 4 bytes + 2 stray bytes
    w.finish();
    auto r = parse_wav(w.bytes, w.size, w.size);
    CHECK(r.ok() && r.value().frames() == 2 && r.value().data_size == 8);
}

NNH_TEST(wav_rejects_what_is_not_a_wav) {
    WavBuilder w = stereo16(4);
    std::memcpy(w.bytes + 8, "AVI ", 4);
    auto r = parse_wav(w.bytes, w.size, w.size);
    CHECK(!r.ok() && r.error().code == ErrorCode::Corrupted);
    CHECK(!parse_wav(w.bytes, 0, 0).ok());
    CHECK(!parse_wav(w.bytes, 11, 11).ok());
}

NNH_TEST(wav_rejects_unsupported_formats) {
    struct Case {
        std::uint32_t format, channels, rate, bits;
    } cases[] = {
        {3, 2, 32000, 32}, // float
        {1, 2, 32000, 24}, // 24-bit
        {1, 3, 32000, 16}, // 3 channels
        {1, 0, 32000, 16},
        {1, 1, 7999, 16},
        {1, 1, 48001, 16},
        {2, 1, 32000, 16}, // ADPCM
    };
    for (const Case& c : cases) {
        WavBuilder w;
        w.header();
        w.fmt(c.format, c.channels, c.rate, c.bits);
        w.chunk("data", 4, 4);
        w.finish();
        auto r = parse_wav(w.bytes, w.size, w.size);
        CHECK(!r.ok() && r.error().code == ErrorCode::Unsupported);
    }
}

NNH_TEST(wav_rejects_broken_structure) {
    { // data before fmt
        WavBuilder w;
        w.header();
        w.chunk("data", 4, 4);
        w.fmt(1, 1, 32000, 16);
        w.finish();
        CHECK(parse_wav(w.bytes, w.size, w.size).error().code == ErrorCode::Corrupted);
    }
    { // no data chunk
        WavBuilder w;
        w.header();
        w.fmt(1, 1, 32000, 16);
        w.finish();
        CHECK(parse_wav(w.bytes, w.size, w.size).error().code == ErrorCode::Corrupted);
    }
    { // a chunk claims more bytes than there are
        WavBuilder w;
        w.header();
        w.chunk("LIST", 0xFFFFFFF0u, 4);
        w.finish();
        CHECK(parse_wav(w.bytes, w.size, w.size).error().code == ErrorCode::Corrupted);
    }
    { // fmt chunk too short
        WavBuilder w;
        w.header();
        w.chunk("fmt ", 14, 14);
        w.chunk("data", 4, 4);
        w.finish();
        CHECK(parse_wav(w.bytes, w.size, w.size).error().code == ErrorCode::Corrupted);
    }
    { // block align that doesn't match channels × bytes
        WavBuilder w = stereo16(2);
        w.bytes[32] = 3;
        CHECK(parse_wav(w.bytes, w.size, w.size).error().code == ErrorCode::Corrupted);
    }
    { // empty data
        WavBuilder w = stereo16(0);
        CHECK(parse_wav(w.bytes, w.size, w.size).error().code == ErrorCode::Corrupted);
    }
    { // header bytes larger than the file: a caller bug, refused
        WavBuilder w = stereo16(2);
        CHECK(parse_wav(w.bytes, w.size, w.size - 1).error().code == ErrorCode::InvalidArgument);
    }
}

NNH_TEST(wav_data_chunk_must_start_inside_the_header_bytes) {
    WavBuilder w;
    w.header();
    w.fmt(1, 1, 32000, 16);
    w.chunk("LIST", 100, 100);
    w.chunk("data", 4, 4);
    w.finish();
    auto r = parse_wav(w.bytes, 60, w.size); // the data chunk header lies past byte 60
    CHECK(!r.ok() && r.error().code == ErrorCode::Corrupted);
}
