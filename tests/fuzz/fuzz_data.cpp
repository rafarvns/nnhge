// Fuzz target body for converted data assets (spec 014 AC-5; see fuzz_targets.hpp): validate, fix the header
// so mutations reach the body, validate again, apply what was accepted into a struct with every field kind
// and check what came out; and apply every input, accepted or not, into a guarded struct (apply must stay in
// bounds anyway).

#include "fuzz_targets.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "core/crc32.hpp"
#include "data/data_apply.hpp"
#include "data/data_format.hpp"
#include "data/data_validate.hpp"
#include "data/schema_fingerprint.hpp"
#include "data/schema_registry.hpp"
#include "nnh/data.hpp"
#include "nnh/gfx.hpp"

namespace nnh::fuzz {

namespace {

using detail::FieldInfo;
using detail::TypeInfo;
namespace fmt = detail::datafmt;

enum class Kind : std::int16_t { Low = -2, Mid = 7, High = 300 };
struct Stats {
    std::uint16_t hp = 5;
    std::int8_t attack = 0;
};
struct All {
    bool flag = false;
    std::int8_t i8 = 0;
    std::uint8_t u8 = 0;
    std::int16_t i16 = 0;
    std::uint16_t u16 = 0;
    std::int32_t i32 = 0;
    std::uint32_t u32 = 0;
    float ratio = 0;
    data::String<8> name;
    Kind kind = Kind::Mid;
    Vec2 at;
    Color tint;
    Stats stats;
    data::Ref<All> next;
    data::TextureRef sprite;
    data::SoundRef sound;
    data::MusicRef music;
    std::int16_t row[3] = {};
    data::List<Stats, 3> party;
    data::List<float, 2> weights;
    Stats pair[2];
};

struct StatsFields {
    using Self = Stats;
    static constexpr FieldInfo list[] = {NNH_DATA_FIELD(hp, .min = 1, .required = true),
                                         NNH_DATA_FIELD(attack)};
};
struct AllFields {
    using Self = All;
    static constexpr FieldInfo list[] = {NNH_DATA_FIELD(flag),
                                         NNH_DATA_FIELD(i8),
                                         NNH_DATA_FIELD(u8, .max = 200),
                                         NNH_DATA_FIELD(i16),
                                         NNH_DATA_FIELD(u16),
                                         NNH_DATA_FIELD(i32),
                                         NNH_DATA_FIELD(u32),
                                         NNH_DATA_FIELD(ratio, .min = -10, .max = 10),
                                         NNH_DATA_FIELD(name),
                                         NNH_DATA_FIELD(kind),
                                         NNH_DATA_FIELD(at),
                                         NNH_DATA_FIELD(tint),
                                         NNH_DATA_FIELD(stats),
                                         NNH_DATA_FIELD(next),
                                         NNH_DATA_FIELD(sprite),
                                         NNH_DATA_FIELD(sound),
                                         NNH_DATA_FIELD(music),
                                         NNH_DATA_FIELD(row),
                                         NNH_DATA_FIELD(party),
                                         NNH_DATA_FIELD(weights, .min = 0),
                                         NNH_DATA_FIELD(pair, .required = true)};
};
constexpr detail::DataEnumValues<3> kKindValues{{-2, 7, 300}};
constexpr detail::EnumInfo kKind = detail::make_enum<Kind>("Kind", "Low, Mid, High", kKindValues, "fuzz");
constexpr TypeInfo kStats = detail::make_type<Stats>("Stats", "", "fuzz", StatsFields::list);
constexpr TypeInfo kAll = detail::make_type<All>("All", "all", "fuzz", AllFields::list);

struct Schema {
    detail::DataRegistry registry;
    detail::TypeNode types[2] = {{&kStats, nullptr}, {&kAll, nullptr}};
    detail::EnumNode kind{&kKind, nullptr};
    std::uint32_t fingerprint = 0;
};

Schema& schema() {
    static Schema s;
    if (s.registry.types == nullptr) {
        add_type(s.registry, s.types[0]);
        add_type(s.registry, s.types[1]);
        add_enum(s.registry, s.kind);
        s.fingerprint = detail::schema_fingerprint(s.registry, kAll);
    }
    return s;
}

void put32(std::uint8_t* p, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * i));
}

/// Copies `data` into `out` with the header's size, CRC and fingerprint made right.
void fix_header(std::uint8_t* out, const std::uint8_t* data, std::size_t size) {
    if (out != data) std::memcpy(out, data, size);
    put32(out + fmt::kSizeAt, static_cast<std::uint32_t>(size));
    put32(out + fmt::kCrcAt, detail::crc32(out + fmt::kHeaderSize, size - fmt::kHeaderSize));
    put32(out + fmt::kFingerprintAt, schema().fingerprint);
}

bool stats_ok(const Stats& s) { return s.hp >= 1; }

/// What an accepted file must have produced.
void check_loaded(const All& a) {
    if (a.u8 > 200 || !std::isfinite(a.ratio) || a.ratio < -10 || a.ratio > 10) std::abort();
    if (a.name.size() > 8 || a.name.c_str()[a.name.size()] != '\0') std::abort();
    if (a.kind != Kind::Low && a.kind != Kind::Mid && a.kind != Kind::High) std::abort();
    if (!std::isfinite(a.at.x) || !std::isfinite(a.at.y)) std::abort();
    if (a.party.size() > 3 || a.weights.size() > 2) std::abort();
    for (const Stats& s : a.party) {
        if (!stats_ok(s)) std::abort();
    }
    for (float w : a.weights) {
        if (!(w >= 0)) std::abort();
    }
    if (!stats_ok(a.pair[0]) || !stats_ok(a.pair[1])) std::abort();
}

/// Apply on anything must not write outside the struct.
void apply_guarded(const std::uint8_t* file, std::size_t size) {
    struct Guarded {
        All all;
        unsigned char guard[32];
    } g;
    std::memset(g.guard, 0xA5, sizeof g.guard);
    kAll.reset(&g.all);
    detail::apply_data(file, size, schema().registry, kAll, &g.all);
    for (unsigned char c : g.guard) {
        if (c != 0xA5) std::abort();
    }
    if (g.all.party.size() > 3 || g.all.weights.size() > 2) std::abort();
}

} // namespace

bool data_fixed_accepted(const std::uint8_t* data, std::size_t size) {
    static std::uint8_t file[fmt::kMaxFileBytes];
    if (size < fmt::kHeaderSize || size > sizeof file) return false;
    fix_header(file, data, size);
    return detail::validate_data(file, size, schema().registry, kAll, schema().fingerprint).ok();
}

int data_target(const std::uint8_t* data, std::size_t size) {
    const Schema& s = schema();
    (void)detail::validate_data(data, size, s.registry, kAll, s.fingerprint);
    static std::uint8_t file[fmt::kMaxFileBytes];
    if (size < fmt::kHeaderSize || size > sizeof file) return 0;
    fix_header(file, data, size);
    apply_guarded(file, size);
    if (!detail::validate_data(file, size, s.registry, kAll, s.fingerprint).ok()) return 0;
    All all;
    kAll.reset(&all);
    detail::apply_data(file, size, s.registry, kAll, &all);
    check_loaded(all);
    return 0;
}

std::size_t data_seed(std::uint8_t* out, std::size_t capacity) {
    // Every field of All set (field indices 0-20), little-endian, built by hand.
    // clang-format off
    static const std::uint8_t body[] = {
        21,                                                // fields
        0,  1,                                             // flag
        1,  0xF6,                                          // i8 = -10
        2,  200,                                           // u8
        3,  0x00, 0x80,                                    // i16 = -32768
        4,  0xFF, 0xFF,                                    // u16
        5,  0x01, 0x00, 0x00, 0x80,                        // i32
        6,  0xFF, 0xFF, 0xFF, 0xFF,                        // u32
        7,  0x00, 0x00, 0x20, 0xC1,                        // ratio = -10
        8,  3,    'a', 0xC3, 0xA7,                         // name = "aç"
        9,  2,                                             // kind = High
        10, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0xC0, // at = (1, -2)
        11, 1,    2,    3,    4,                           // tint
        12, 2,    0,    9,    0,    1,    0xFF,            // stats {hp 9, attack -1}
        13, 1,    0,    0,    0,                           // next
        14, 2,    0,    0,    0,                           // sprite
        15, 3,    0,    0,    0,                           // sound
        16, 4,    0,    0,    0,                           // music
        17, 1,    0,    2,    0,    3,    0,               // row
        18, 2,    0,    1,    0,    1,    0,    1,    0,    2, 0, // party: 2 x {hp}
        19, 1,    0,    0x00, 0x00, 0x00, 0x3F,            // weights = [0.5]
        20, 1,    0,    7,    0,    1,    0,    8,    0,    // pair
    };
    // clang-format on
    const std::size_t size = fmt::kHeaderSize + sizeof body;
    if (capacity < size) return 0;
    std::memset(out, 0, fmt::kHeaderSize);
    std::memcpy(out, fmt::kMagic, 4);
    out[fmt::kVersionAt] = static_cast<std::uint8_t>(fmt::kVersion);
    std::memcpy(out + fmt::kHeaderSize, body, sizeof body);
    fix_header(out, out, size);
    return size;
}

} // namespace nnh::fuzz
