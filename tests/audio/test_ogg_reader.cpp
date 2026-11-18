// The Ogg Vorbis reader (audio/ogg_reader.hpp, spec 008 FR-1, FR-3, FR-10): format, length, LOOPSTART,
// decoding in any read size, sample-exact seeking (loop points), and errors. Fixtures come from
// tests/data/sounds/make_sounds.py (sine sweeps: every position sounds different).

#include <cmath>
#include <cstdio>
#include <cstring>

#include "audio/ogg_reader.hpp"
#include "test.hpp"

using nnh::ErrorCode;
using nnh::detail::ByteSource;
using nnh::detail::OggDecoder;

namespace {

constexpr std::size_t kMaxFile = 64 * 1024;
constexpr std::uint64_t kMonoFrames = 16 * 22050;
constexpr std::uint64_t kStereoFrames = 32000;

alignas(16) unsigned char g_arena[OggDecoder::kDefaultArenaBytes];
alignas(16) unsigned char g_arena2[OggDecoder::kDefaultArenaBytes]; // for a second open decoder

struct MemoryFile {
    unsigned char bytes[kMaxFile];
    std::size_t size = 0;
    std::size_t fail_after = static_cast<std::size_t>(-1); ///< reads at or past this offset fail
    std::size_t bytes_read = 0;
};

MemoryFile g_file;

bool load(const char* name) {
    char path[512];
    std::snprintf(path, sizeof path, "%s/sounds/%s", NNH_TEST_DATA_DIR, name);
    std::FILE* f = std::fopen(path, "rb");
    if (f == nullptr) return false;
    g_file.size = std::fread(g_file.bytes, 1, kMaxFile, f);
    g_file.fail_after = static_cast<std::size_t>(-1);
    std::fclose(f);
    return g_file.size > 0 && g_file.size < kMaxFile;
}

std::size_t read_memory(void* context, std::uint64_t offset, void* out, std::size_t n) {
    auto* file = static_cast<MemoryFile*>(context);
    if (offset >= file->size || offset >= file->fail_after) return 0;
    const std::size_t available = file->size - static_cast<std::size_t>(offset);
    const std::size_t count = n < available ? n : available;
    std::memcpy(out, file->bytes + offset, count);
    file->bytes_read += count;
    return count;
}

ByteSource source() { return {&g_file, g_file.size, read_memory}; }

bool open(OggDecoder& decoder, const char* name, unsigned char* arena = g_arena) {
    return load(name) && decoder.open(source(), arena, OggDecoder::kDefaultArenaBytes).ok();
}

/// Decodes everything left into `out` (capacity `max` frames) in reads of `step` frames.
std::uint64_t decode_all(OggDecoder& decoder, std::int16_t* out, std::uint64_t max, std::uint32_t step) {
    std::uint64_t total = 0;
    for (;;) {
        const std::uint32_t want = static_cast<std::uint32_t>(max - total < step ? max - total : step);
        if (want == 0) return total;
        auto got = decoder.read(out + total * static_cast<std::uint64_t>(decoder.channels()), want);
        if (!got.ok() || got.value() == 0) return total;
        total += got.value();
    }
}

std::int16_t g_reference[kMonoFrames];
std::int16_t g_other[kMonoFrames];

} // namespace

NNH_TEST(ogg_reads_the_format_and_the_length) {
    OggDecoder mono;
    CHECK(open(mono, "sweep_mono_22k.ogg"));
    CHECK(mono.channels() == 1 && mono.rate() == 22050 && mono.length() == kMonoFrames);
    CHECK(!mono.has_loop_start());
    OggDecoder stereo;
    CHECK(load("sweep_stereo_32k_loop.ogg"));
    CHECK(stereo.open(source(), g_arena2, sizeof g_arena2).ok());
    CHECK(stereo.channels() == 2 && stereo.rate() == 32000 && stereo.length() == kStereoFrames);
    CHECK(stereo.has_loop_start() && stereo.loop_start() == 12345);
}

NNH_TEST(ogg_decodes_the_whole_sweep) {
    OggDecoder decoder;
    CHECK(open(decoder, "sweep_mono_22k.ogg"));
    const std::uint64_t frames = decode_all(decoder, g_reference, kMonoFrames, 4096);
    CHECK(frames == kMonoFrames && decoder.position() == kMonoFrames);
    auto end = decoder.read(g_other, 16);
    CHECK(end.ok() && end.value() == 0);
    // Lossy, but close to the synthesized sweep (amplitude 0.5, 200 -> 3000 Hz over 16 s).
    int far = 0;
    for (std::uint64_t i = 0; i < kMonoFrames; i += 97) {
        const double t = static_cast<double>(i) / 22050.0;
        const double phase = 2 * 3.14159265358979 * (200 * t + 2800 * t * t / 32.0);
        const double expected = 0.5 * std::sin(phase) * 32767.0;
        if (std::fabs(expected - g_reference[i]) > 0.05 * 32767.0) ++far;
    }
    CHECK(far < 20);
}

NNH_TEST(ogg_read_size_does_not_change_the_samples) {
    OggDecoder a;
    CHECK(open(a, "sweep_mono_22k.ogg"));
    CHECK(decode_all(a, g_reference, kMonoFrames, 4096) == kMonoFrames);
    OggDecoder b;
    CHECK(open(b, "sweep_mono_22k.ogg", g_arena2));
    CHECK(decode_all(b, g_other, kMonoFrames, 37) == kMonoFrames);
    CHECK(std::memcmp(g_reference, g_other, sizeof g_reference) == 0);
}

NNH_TEST(ogg_seek_lands_on_the_exact_sample) {
    OggDecoder linear;
    CHECK(open(linear, "sweep_mono_22k.ogg"));
    CHECK(decode_all(linear, g_reference, kMonoFrames, 4096) == kMonoFrames);
    const std::uint64_t targets[] = {0, 1, 777, 5000, 40000, 100000, 200001, 351000, kMonoFrames - 10};
    OggDecoder decoder;
    CHECK(open(decoder, "sweep_mono_22k.ogg", g_arena2));
    for (std::uint64_t target : targets) {
        CHECK(decoder.seek(target).ok());
        CHECK(decoder.position() == target);
        const std::uint64_t want = kMonoFrames - target < 2000 ? kMonoFrames - target : 2000;
        const std::uint64_t got = decode_all(decoder, g_other, want, 300);
        CHECK(got == want);
        CHECK(std::memcmp(g_other, g_reference + target, static_cast<std::size_t>(got) * 2) == 0);
    }
}

NNH_TEST(ogg_loops_from_the_end_to_the_loop_point) {
    OggDecoder linear;
    CHECK(open(linear, "sweep_stereo_32k_loop.ogg"));
    CHECK(decode_all(linear, g_reference, kStereoFrames, 4096) == kStereoFrames);
    OggDecoder decoder;
    CHECK(open(decoder, "sweep_stereo_32k_loop.ogg", g_arena2));
    for (int pass = 0; pass < 3; ++pass) { // seeking again reuses the first search
        const std::uint64_t left = pass == 0 ? kStereoFrames : kStereoFrames - 12345 - 500;
        CHECK(decode_all(decoder, g_other, kStereoFrames, 1000) == left);
        CHECK(decoder.seek(decoder.loop_start()).ok());
        CHECK(decode_all(decoder, g_other, 500, 500) == 500);
        CHECK(std::memcmp(g_other, g_reference + 12345 * 2, 500 * 2 * 2) == 0);
        CHECK(decoder.seek(12345 + 500).ok()); // where it already is: nothing to do
        CHECK(decoder.position() == 12345 + 500);
    }
}

NNH_TEST(ogg_seek_reads_from_the_page_not_from_the_start) {
    OggDecoder decoder;
    CHECK(open(decoder, "sweep_mono_22k.ogg"));
    g_file.bytes_read = 0;
    CHECK(decoder.seek(kMonoFrames - 2000).ok());
    CHECK(g_file.bytes_read < g_file.size / 2); // page headers + the last pages only
}

NNH_TEST(ogg_seek_past_the_end_is_refused) {
    OggDecoder decoder;
    CHECK(open(decoder, "sweep_stereo_32k_loop.ogg"));
    auto r = decoder.seek(kStereoFrames + 1);
    CHECK(!r.ok() && r.error().code == ErrorCode::InvalidArgument);
    CHECK(decoder.seek(kStereoFrames).ok()); // the end itself: next read returns 0
    auto end = decoder.read(g_other, 10);
    CHECK(end.ok() && end.value() == 0);
}

NNH_TEST(ogg_rejects_what_is_not_ogg_vorbis) {
    OggDecoder decoder;
    for (const char* name : {"bad_not_ogg.ogg", "ok_mono16.wav"}) {
        CHECK(load(name));
        auto r = decoder.open(source(), g_arena, sizeof g_arena);
        CHECK(!r.ok() && r.error().code == ErrorCode::Corrupted);
        CHECK(!decoder.is_open());
    }
    CHECK(load("sweep_mono_22k.ogg"));
    g_file.size = 100; // the headers are cut off
    auto cut = decoder.open(source(), g_arena, sizeof g_arena);
    CHECK(!cut.ok() && cut.error().code == ErrorCode::Corrupted);
}

NNH_TEST(ogg_needs_enough_arena) {
    OggDecoder decoder;
    CHECK(load("sweep_mono_22k.ogg"));
    auto r = decoder.open(source(), g_arena, 4096);
    CHECK(!r.ok() && r.error().code == ErrorCode::OutOfMemory);
    CHECK(decoder.open(source(), g_arena, sizeof g_arena).ok());
    CHECK(decoder.arena_used() > 4096 && decoder.arena_used() <= sizeof g_arena);
}

NNH_TEST(ogg_a_file_cut_short_plays_up_to_its_last_whole_page) {
    OggDecoder decoder;
    CHECK(load("sweep_mono_22k.ogg"));
    g_file.size = g_file.size * 6 / 10; // ends in the middle of a page
    CHECK(decoder.open(source(), g_arena, sizeof g_arena).ok());
    CHECK(decoder.length() > 0 && decoder.length() < kMonoFrames);
    CHECK(decode_all(decoder, g_other, kMonoFrames, 4096) == decoder.length());
}

NNH_TEST(ogg_seek_into_a_damaged_part_is_an_error) {
    // Found by libFuzzer: the last page promises more frames than the damaged file can decode; seeking
    // there must fail instead of "succeeding" at an earlier frame.
    OggDecoder decoder;
    CHECK(load("sweep_mono_22k.ogg"));
    std::size_t last_page = g_file.size - 4;
    while (last_page > 0 && std::memcmp(g_file.bytes + last_page, "OggS", 4) != 0) --last_page;
    std::memset(g_file.bytes + g_file.size / 3, 0, last_page - g_file.size / 3); // keep the last page
    CHECK(decoder.open(source(), g_arena, sizeof g_arena).ok());
    CHECK(decoder.length() == kMonoFrames);
    auto r = decoder.seek(kMonoFrames - 100);
    CHECK(!r.ok() && r.error().code == ErrorCode::Corrupted);
}

NNH_TEST(ogg_a_failing_read_is_an_io_error) {
    OggDecoder decoder;
    CHECK(open(decoder, "sweep_mono_22k.ogg"));
    g_file.fail_after = g_file.size / 2;
    std::uint64_t total = 0;
    nnh::Result<std::uint32_t> r = std::uint32_t{0};
    do {
        r = decoder.read(g_other, 4096);
        if (r.ok()) total += r.value();
    } while (r.ok() && r.value() > 0);
    CHECK(!r.ok() && r.error().code == ErrorCode::IoError);
    CHECK(total < kMonoFrames);
}

NNH_TEST(ogg_closed_decoder_reads_nothing) {
    OggDecoder decoder;
    CHECK(!decoder.is_open());
    auto r = decoder.read(g_other, 10);
    CHECK(!r.ok() && r.error().code == ErrorCode::InvalidArgument);
    CHECK(open(decoder, "sweep_mono_22k.ogg"));
    decoder.close();
    CHECK(!decoder.is_open() && decoder.channels() == 0);
}
