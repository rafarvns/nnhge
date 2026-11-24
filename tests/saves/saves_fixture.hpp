#pragma once
// Shared types and helpers of the structured-save tests (spec 027). The types are declared here, in a header and in
// a game namespace (the "declarations in a header" pattern); tests that need private types declare them in their own
// source. Enum names come from the tests' data schema (test_data::Element, NNH_DATA_ENUM in
// tests/data_assets/test_types.cpp): adding an NNH_DATA_ENUM to nnh_tests would change spec 014's registry tests.

#include <cstdint>
#include <cstring>
#include <vector>

#include "../data_assets/test_types.hpp"
#include "nnh/app.hpp"
#include "nnh/gfx.hpp"
#include "nnh/platform_fake.hpp"
#include "nnh/saves.hpp"
#include "test.hpp"

namespace saves_test {

enum class Mood : std::uint8_t { Calm, Angry, Sleepy }; // no names registered: any std::uint8_t value loads
enum class Big : std::int64_t { Low = -5, High = 1ll << 40 };
struct Target {};

struct Pair {
    std::uint8_t a = 1;
    std::int16_t b = -2;
};
NNH_SAVE_STRUCT(Pair, NNH_SAVE_FIELD(a, 1), NNH_SAVE_FIELD(b, 2));

/// One of every one-value kind, declared out of id order.
struct Kinds {
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
    nnh::data::String<8> name{};
    Pair pair{};
    Mood mood = Mood::Calm;
    test_data::Element element = test_data::Element::Normal;
    Big big = Big::Low;
};
NNH_SAVE_TYPE(Kinds, "kinds", NNH_SAVE_FIELD(big, 22), NNH_SAVE_FIELD(mood, 20), NNH_SAVE_FIELD(flag, 1),
              NNH_SAVE_FIELD(u8, 2), NNH_SAVE_FIELD(i8, 3), NNH_SAVE_FIELD(u16, 4), NNH_SAVE_FIELD(i16, 5),
              NNH_SAVE_FIELD(u32, 6), NNH_SAVE_FIELD(i32, 7), NNH_SAVE_FIELD(u64, 8), NNH_SAVE_FIELD(i64, 9),
              NNH_SAVE_FIELD(f32, 10), NNH_SAVE_FIELD(f64, 11), NNH_SAVE_FIELD(v2, 12), NNH_SAVE_FIELD(color, 13),
              NNH_SAVE_FIELD(ref, 14), NNH_SAVE_FIELD(tex, 15), NNH_SAVE_FIELD(snd, 16), NNH_SAVE_FIELD(mus, 17),
              NNH_SAVE_FIELD(name, 18), NNH_SAVE_FIELD(pair, 19), NNH_SAVE_FIELD(element, 21));

/// Every sequence shape.
struct Seqs {
    std::uint16_t numbers[3] = {};
    nnh::data::List<bool, 4> flags;
    nnh::data::List<nnh::data::String<4>, 3> words;
    nnh::data::List<Pair, 4> pairs;
    Pair grid[2];
};
NNH_SAVE_TYPE(Seqs, "seqs", NNH_SAVE_FIELD(numbers, 1), NNH_SAVE_FIELD(flags, 2), NNH_SAVE_FIELD(words, 3),
              NNH_SAVE_FIELD(pairs, 4), NNH_SAVE_FIELD(grid, 5));

using Bytes = std::vector<std::uint8_t>;

/// The structured header of `key` with `version`, as the tests write it by hand: "NNSV", format 1, key length,
/// flags 0, version, FNV-1a 32 of the key, the key.
inline Bytes header(const char* key, std::uint32_t version) {
    const std::uint32_t hash = nnh::detail::save_key_hash(key);
    const std::size_t n = std::strlen(key);
    Bytes b = {'N', 'N', 'S', 'V', 1, static_cast<std::uint8_t>(n), 0, 0};
    for (int i = 0; i < 4; ++i) b.push_back(static_cast<std::uint8_t>(version >> (8 * i)));
    for (int i = 0; i < 4; ++i) b.push_back(static_cast<std::uint8_t>(hash >> (8 * i)));
    b.insert(b.end(), key, key + n);
    return b;
}

inline Bytes operator+(Bytes a, const Bytes& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

/// Encodes `value` into a vector (empty on failure).
template <class T> Bytes encoded(const T& value) {
    Bytes out(nnh::save::max_size<T>());
    const auto r = nnh::save::encode(value, out.data(), out.size());
    if (!r) return {};
    out.resize(r.value());
    return out;
}

/// A fresh engine on the fake platform (files cleared).
inline void fresh() {
    nnh::shutdown();
    nnh::platform_fake::reset();
    CHECK(nnh::init());
}

inline bool same_bytes(const void* a, const void* b, std::size_t n) { return std::memcmp(a, b, n) == 0; }

} // namespace saves_test
