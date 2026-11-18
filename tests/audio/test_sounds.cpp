// Loading sounds (nnh/audio.hpp load_sound, spec 008 FR-1, FR-2): by asset path without extension, WAV
// or Ogg Vorbis decoded to 16-bit frames in platform memory, every error, and release.

#include <cstring>
#include <utility>

#include "audio/audio_internal.hpp"
#include "audio/ogg_reader.hpp"
#include "audio_test_util.hpp"
#include "nnh/audio.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;
using nnh::ErrorCode;

namespace {

std::uint8_t g_stereo16[64 + 4 * 4];
std::uint8_t g_mono8[64 + 3];
std::uint8_t g_24bit[64 + 6];
std::uint8_t g_ogg[16 * 1024];
std::size_t g_ogg_size = 0;
std::uint8_t g_huge[64 + 4 * 1024 * 1024 + 4]; // just over the 4 MB limit once decoded

constexpr char kIndex[] = "sfx/both.ogg\nsfx/both.wav\nsfx/broken.wav\nsfx/deep.wav\nsfx/huge.wav\n"
                          "sfx/jump.wav\nsfx/loop.ogg\nsfx/tick.wav\n";

void start(nnh::AppConfig config = {}) {
    nnh::shutdown();
    fake::reset();
    const std::size_t stereo = audio_test::make_wav(g_stereo16, 2, 16, 32000, 4, [](std::uint32_t i) {
        return static_cast<int>(i) * 1000 - 3000; // -3000, -2000, ... 4000
    });
    const std::size_t mono8 = audio_test::make_wav(g_mono8, 1, 8, 8000, 3, [](std::uint32_t i) {
        return i == 0 ? 0x80 : (i == 1 ? 0xFF : 0x00);
    });
    const std::size_t deep = audio_test::make_wav(g_24bit, 1, 16, 8000, 2, [](std::uint32_t) { return 0; });
    g_24bit[34] = 24; // now claims 24-bit samples
    g_24bit[32] = 3;
    const std::size_t huge = audio_test::make_wav(g_huge, 1, 16, 8000, 2 * 1024 * 1024 + 2, [](std::uint32_t) { return 0; });
    g_ogg_size = audio_test::load_fixture("sweep_stereo_32k_loop.ogg", g_ogg, sizeof g_ogg);
    CHECK(g_ogg_size > 0);
    fake::add_asset(".nnh_assets", kIndex, sizeof kIndex - 1);
    fake::add_asset("sfx/jump.wav", g_stereo16, stereo);
    fake::add_asset("sfx/tick.wav", g_mono8, mono8);
    fake::add_asset("sfx/deep.wav", g_24bit, deep);
    fake::add_asset("sfx/broken.wav", "RIFF\0\0\0\0WAVEjunk", 16);
    fake::add_asset("sfx/huge.wav", g_huge, huge);
    fake::add_asset("sfx/loop.ogg", g_ogg, g_ogg_size);
    fake::add_asset("sfx/both.wav", g_mono8, mono8);
    fake::add_asset("sfx/both.ogg", g_ogg, g_ogg_size);
    CHECK(nnh::init(config));
}

const std::int16_t* platform_frames(const nnh::Sound& sound) {
    return fake::sample_frames(nnh::detail::sound_sample(sound));
}

} // namespace

NNH_TEST(a_wav_sound_loads_into_platform_memory) {
    start();
    auto loaded = nnh::load_sound("sfx/jump");
    CHECK(loaded.ok());
    nnh::Sound sound = std::move(loaded.value());
    CHECK(sound.valid());
    CHECK(sound.duration() > 0.000124f && sound.duration() < 0.000126f); // 4 frames at 32 kHz
    const std::int16_t* frames = platform_frames(sound);
    CHECK(frames != nullptr && frames[0] == -3000 && frames[7] == 4000);
    CHECK(fake::state().live_samples == 1 && fake::state().samples_ready == 1);
    CHECK(nnh::mem::tag_stats(nnh::detail::audio_memory_tag()).bytes >= 16);
    nnh::shutdown();
}

NNH_TEST(an_8_bit_wav_becomes_16_bit) {
    start();
    auto sound = nnh::load_sound("sfx/tick");
    CHECK(sound.ok());
    const std::int16_t* frames = platform_frames(sound.value());
    CHECK(frames != nullptr && frames[0] == 0 && frames[1] == 127 * 256 && frames[2] == -32768);
    nnh::shutdown();
}

NNH_TEST(an_ogg_sound_is_decoded_whole) {
    start();
    auto loaded = nnh::load_sound("sfx/loop");
    CHECK(loaded.ok());
    CHECK(loaded.value().duration() > 0.999f && loaded.value().duration() < 1.001f);
    // The same frames the reader decodes.
    static std::int16_t expected[2 * 32000];
    alignas(16) static unsigned char arena[nnh::detail::OggDecoder::kDefaultArenaBytes];
    static nnh::detail::OggDecoder decoder;
    struct Mem {
        const std::uint8_t* d;
        std::size_t n;
    } mem{g_ogg, g_ogg_size};
    auto read = [](void* c, std::uint64_t o, void* out, std::size_t n) -> std::size_t {
        auto* m = static_cast<Mem*>(c);
        const std::size_t k = n < m->n - o ? n : static_cast<std::size_t>(m->n - o);
        std::memcpy(out, m->d + o, k);
        return k;
    };
    CHECK(decoder.open({&mem, g_ogg_size, read}, arena, sizeof arena).ok());
    auto got = decoder.read(expected, 32000);
    CHECK(got.ok() && got.value() == 32000);
    CHECK(std::memcmp(platform_frames(loaded.value()), expected, sizeof expected) == 0);
    decoder.close();
    nnh::shutdown();
}

NNH_TEST(wav_is_tried_before_ogg) {
    start();
    auto sound = nnh::load_sound("sfx/both");
    CHECK(sound.ok() && sound.value().duration() < 0.001f); // the 3-frame WAV, not the 1 s Ogg
    nnh::shutdown();
}

NNH_TEST(sound_errors_are_reported) {
    start();
    CHECK(nnh::load_sound("sfx/missing").error().code == ErrorCode::NotFound);
    CHECK(nnh::load_sound("sfx/jump.wav").error().code == ErrorCode::NotFound); // no extension in the name
    CHECK(nnh::load_sound("../sfx/jump").error().code == ErrorCode::InvalidArgument);
    CHECK(nnh::load_sound(nullptr).error().code == ErrorCode::InvalidArgument);
    CHECK(nnh::load_sound("sfx/broken").error().code == ErrorCode::Corrupted);
    CHECK(nnh::load_sound("sfx/deep").error().code == ErrorCode::Unsupported);
    CHECK(nnh::load_sound("sfx/huge").error().code == ErrorCode::LimitReached);
    CHECK(fake::state().live_samples == 0);
    nnh::shutdown();
    CHECK(nnh::load_sound("sfx/jump").error().code == ErrorCode::Unsupported); // before init
}

NNH_TEST(the_sound_table_has_a_limit) {
    start({.audio = {.max_sounds = 2}});
    auto a = nnh::load_sound("sfx/jump");
    auto b = nnh::load_sound("sfx/tick");
    auto c = nnh::load_sound("sfx/jump");
    CHECK(a.ok() && b.ok());
    CHECK(!c.ok() && c.error().code == ErrorCode::LimitReached);
    CHECK(fake::state().live_samples == 2);
    nnh::shutdown();
}

NNH_TEST(a_sound_is_freed_by_reset_and_by_shutdown) {
    start();
    nnh::Sound a = std::move(nnh::load_sound("sfx/jump").value());
    nnh::Sound b = std::move(nnh::load_sound("sfx/tick").value());
    a.reset();
    CHECK(!a.valid() && fake::state().live_samples == 1);
    nnh::Sound moved = std::move(b);
    CHECK(moved.valid() && !b.valid());
    nnh::shutdown();
    CHECK(fake::state().live_samples == 0);
    CHECK(!moved.valid());
    moved.reset(); // harmless after shutdown
    CHECK(nnh::mem::tag_stats(nnh::detail::audio_memory_tag()).bytes == 0);
}
