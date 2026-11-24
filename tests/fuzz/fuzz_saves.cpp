// Fuzz target of structured saves (fuzz_saves.hpp).

#include "fuzz_saves.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/crc32.hpp"
#include "fs/save_format.hpp"
#include "nnh/gfx.hpp"
#include "nnh/platform/fs.hpp"
#include "nnh/saves.hpp"

namespace fuzz_saves {

enum class Mood : std::uint8_t { Calm, Angry };
struct Target {};

struct Pair {
    std::uint8_t a = 1;
    std::int16_t b = -2;
    nnh::data::List<std::uint16_t, 3> small;
};
NNH_SAVE_STRUCT(Pair, NNH_SAVE_FIELD(a, 1), NNH_SAVE_FIELD(b, 2), NNH_SAVE_FIELD(small, 3));

struct Row {
    std::uint32_t id = 0;
    nnh::data::String<6> label;
    Pair pair;
    float wear = 0;
};
NNH_SAVE_STRUCT(Row, NNH_SAVE_FIELD(id, 1), NNH_SAVE_FIELD(label, 2), NNH_SAVE_FIELD(pair, 3), NNH_SAVE_FIELD(wear, 5),
                NNH_SAVE_RETIRED(4));

/// Every kind and shape (ids 1-30, some retired).
struct All {
    bool flag = false;
    std::uint8_t u8 = 0;
    std::int8_t i8 = 0;
    std::uint16_t u16 = 0;
    std::int16_t i16 = 0;
    std::uint32_t u32 = 0;
    std::int32_t i32 = 0;
    std::uint64_t u64 = 0;
    std::int64_t i64 = 0;
    float f32 = 0;
    double f64 = 0;
    nnh::Vec2 v2{};
    nnh::Color color{};
    nnh::data::Ref<Target> ref{};
    nnh::data::TextureRef tex{};
    nnh::data::SoundRef snd{};
    nnh::data::MusicRef mus{};
    nnh::data::String<12> name;
    Pair pair;
    Mood mood = Mood::Calm;
    bool flags[9] = {};
    std::int32_t numbers[4] = {};
    nnh::data::List<double, 3> doubles;
    nnh::data::List<nnh::data::String<5>, 4> words;
    nnh::data::List<Row, 5> rows;
    Pair pairs[2];
    nnh::data::List<std::uint8_t, 16> blob;
};
NNH_SAVE_TYPE(All, "fuzz", NNH_SAVE_FIELD(flag, 1), NNH_SAVE_FIELD(u8, 2), NNH_SAVE_FIELD(i8, 3),
              NNH_SAVE_FIELD(u16, 4), NNH_SAVE_FIELD(i16, 5), NNH_SAVE_FIELD(u32, 6), NNH_SAVE_FIELD(i32, 7),
              NNH_SAVE_FIELD(u64, 8), NNH_SAVE_FIELD(i64, 9), NNH_SAVE_FIELD(f32, 10), NNH_SAVE_FIELD(f64, 11),
              NNH_SAVE_FIELD(v2, 12), NNH_SAVE_FIELD(color, 13), NNH_SAVE_FIELD(ref, 14), NNH_SAVE_FIELD(tex, 15),
              NNH_SAVE_FIELD(snd, 16), NNH_SAVE_FIELD(mus, 17), NNH_SAVE_FIELD(name, 18), NNH_SAVE_FIELD(pair, 19),
              NNH_SAVE_FIELD(mood, 20), NNH_SAVE_FIELD(flags, 21), NNH_SAVE_FIELD(numbers, 22),
              NNH_SAVE_FIELD(doubles, 23), NNH_SAVE_FIELD(words, 24), NNH_SAVE_FIELD(rows, 25),
              NNH_SAVE_FIELD(pairs, 26), NNH_SAVE_FIELD(blob, 30), NNH_SAVE_RETIRED(27, 28));

} // namespace fuzz_saves

namespace nnh::fuzz {
namespace {

using fuzz_saves::All;

constexpr std::size_t kMax = nnh::save::max_size<All>();
constexpr std::size_t kInputLimit = 64 * 1024;
std::uint8_t g_a[kMax];
std::uint8_t g_b[kMax];
std::uint8_t g_work[kInputLimit + 64];

void fail(const char* what) {
    std::fprintf(stderr, "saves_target: %s\n", what);
    std::abort();
}

void ignore_line(void*, const char*) {}

/// An accepted value: encode, decode, encode again — the same bytes.
void round_trip(const All& value) {
    const auto first = nnh::save::encode(value, g_a, sizeof g_a);
    if (!first) fail("an accepted value doesn't encode");
    static All again;
    if (!nnh::save::decode(g_a, first.value(), again)) fail("a re-encoded value doesn't decode");
    const auto second = nnh::save::encode(again, g_b, sizeof g_b);
    if (!second || second.value() != first.value() || std::memcmp(g_a, g_b, first.value()) != 0) {
        fail("encode(decode(encode(v))) differs from encode(v)");
    }
}

/// The migration view over an accepted payload, on paths taken from the input.
void view(const std::uint8_t* payload, std::size_t size, const std::uint8_t* data, std::size_t data_size) {
    const std::size_t header = 16 + 4; // "fuzz"
    if (size < header) return;
    const nnh::save::OldFields old(payload + header, static_cast<std::uint32_t>(size - header));
    for (std::size_t i = 0; i + 3 < data_size && i < 64; i += 4) {
        nnh::save::FieldPath p = nnh::save::path(data[i] % 32);
        if ((data[i + 1] & 1) != 0) p = p.item(data[i + 2] % 8);
        if ((data[i + 1] & 2) != 0) p = p.field(static_cast<std::uint16_t>(data[i + 3] % 6));
        (void)old.has(p);
        (void)old.kind(p);
        (void)old.shape(p);
        (void)old.count(p);
        std::uint32_t number = 0;
        nnh::data::String<8> text;
        fuzz_saves::Pair pair;
        nnh::data::List<fuzz_saves::Row, 5> rows;
        (void)old.get(p, number);
        (void)old.get(p, text);
        (void)old.get(p, pair);
        (void)old.get(p, rows);
    }
}

bool decode_payload(const std::uint8_t* payload, std::size_t size, const std::uint8_t* data, std::size_t data_size) {
    static All value;
    if (!nnh::save::decode(payload, size, value)) return false;
    round_trip(value);
    view(payload, size, data, data_size);
    (void)nnh::save::dump_bytes<All>(payload, size, ignore_line);
    return true;
}

/// The input as a whole file: NNHS header with its size and CRC fixed, then nnh::save::load from the fake card.
void load_file(const std::uint8_t* payload, std::size_t size) {
    std::uint8_t header[detail::kSaveHeaderSize];
    detail::build_save_header(header, static_cast<std::uint32_t>(size), 1, detail::crc32(payload, size));
    const platform::FsChunk chunks[2] = {{header, sizeof header}, {payload, size}};
    if (!platform::fs_write("fuzz.sav", chunks, 2)) return;
    static All value;
    if (nnh::save::load("fuzz.sav", value, g_a, sizeof g_a)) round_trip(value);
    platform::fs_remove("fuzz.sav");
    platform::fs_remove("fuzz.sav.bak");
}

} // namespace

bool saves_accepted(const std::uint8_t* data, std::size_t size) {
    static All value;
    return data != nullptr && nnh::save::decode(data, size, value).ok();
}

int saves_target(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size > kInputLimit) return 0;
    decode_payload(data, size, data, size);
    // the bytes as fields behind a valid header, so mutations reach the structure
    static constexpr std::uint8_t kHeader[20] = {'N', 'N', 'S', 'V', 1, 4, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
                                                 'f', 'u', 'z', 'z'};
    std::memcpy(g_work, kHeader, sizeof kHeader);
    const std::uint32_t hash = detail::save_key_hash("fuzz");
    std::memcpy(g_work + 12, &hash, 4);
    if (size > 0) std::memcpy(g_work + sizeof kHeader, data, size);
    decode_payload(g_work, sizeof kHeader + size, data, size);
    load_file(data, size);
    return 0;
}

std::size_t saves_seed(bool full, std::uint8_t* out, std::size_t capacity) {
    All a;
    if (full) {
        a.flag = true;
        a.u8 = 200;
        a.i8 = -100;
        a.u16 = 60000;
        a.i16 = -30000;
        a.u32 = 4000000000u;
        a.i32 = -2000000000;
        a.u64 = 1ull << 63;
        a.i64 = -(1ll << 62);
        a.f32 = 0.5f;
        a.f64 = -1.25;
        a.v2 = {1, 2};
        a.color = {1, 2, 3, 4};
        a.ref.id = 5;
        a.tex.id = 6;
        a.snd.id = 7;
        a.mus.id = 8;
        a.name.assign("caf\xC3\xA9");
        a.pair.small.push_back(9);
        a.mood = fuzz_saves::Mood::Angry;
        a.flags[3] = true;
        a.numbers[1] = -7;
        a.doubles.push_back(3.5);
        nnh::data::String<5> w;
        w.assign("ab");
        a.words.push_back(w);
        a.words.push_back(w);
        fuzz_saves::Row r;
        r.label.assign("row");
        r.pair.small.push_back(4);
        a.rows.push_back(r);
        a.rows.push_back(r);
        a.blob.push_back(0xFF);
    }
    const auto n = nnh::save::encode(a, out, capacity);
    return n.ok() ? n.value() : 0;
}

} // namespace nnh::fuzz
