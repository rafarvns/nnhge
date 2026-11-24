// Cost (spec 027 T5.5, FR-8, FR-19): nothing allocates over store, load, encode, decode, steps, the view and the
// dump; the decoder visits each value once (linear in the file: a 128-field struct, a 256-item list of rows).

#include <string>

#include "alloc_counter.hpp"
#include "migration_types.hpp"
#include "saves/save_apply.hpp"
#include "saves_fixture.hpp"

using saves_test::Bytes;
using saves_test::encoded;
using saves_test::fresh;

namespace cost {

struct Wide {
    std::uint8_t f1 = 1;
    std::uint8_t f2 = 2;
    std::uint8_t f3 = 3;
    std::uint8_t f4 = 4;
    std::uint8_t f5 = 5;
    std::uint8_t f6 = 6;
    std::uint8_t f7 = 0;
    std::uint8_t f8 = 1;
    std::uint8_t f9 = 2;
    std::uint8_t f10 = 3;
    std::uint8_t f11 = 4;
    std::uint8_t f12 = 5;
    std::uint8_t f13 = 6;
    std::uint8_t f14 = 0;
    std::uint8_t f15 = 1;
    std::uint8_t f16 = 2;
    std::uint8_t f17 = 3;
    std::uint8_t f18 = 4;
    std::uint8_t f19 = 5;
    std::uint8_t f20 = 6;
    std::uint8_t f21 = 0;
    std::uint8_t f22 = 1;
    std::uint8_t f23 = 2;
    std::uint8_t f24 = 3;
    std::uint8_t f25 = 4;
    std::uint8_t f26 = 5;
    std::uint8_t f27 = 6;
    std::uint8_t f28 = 0;
    std::uint8_t f29 = 1;
    std::uint8_t f30 = 2;
    std::uint8_t f31 = 3;
    std::uint8_t f32 = 4;
    std::uint8_t f33 = 5;
    std::uint8_t f34 = 6;
    std::uint8_t f35 = 0;
    std::uint8_t f36 = 1;
    std::uint8_t f37 = 2;
    std::uint8_t f38 = 3;
    std::uint8_t f39 = 4;
    std::uint8_t f40 = 5;
    std::uint8_t f41 = 6;
    std::uint8_t f42 = 0;
    std::uint8_t f43 = 1;
    std::uint8_t f44 = 2;
    std::uint8_t f45 = 3;
    std::uint8_t f46 = 4;
    std::uint8_t f47 = 5;
    std::uint8_t f48 = 6;
    std::uint8_t f49 = 0;
    std::uint8_t f50 = 1;
    std::uint8_t f51 = 2;
    std::uint8_t f52 = 3;
    std::uint8_t f53 = 4;
    std::uint8_t f54 = 5;
    std::uint8_t f55 = 6;
    std::uint8_t f56 = 0;
    std::uint8_t f57 = 1;
    std::uint8_t f58 = 2;
    std::uint8_t f59 = 3;
    std::uint8_t f60 = 4;
    std::uint8_t f61 = 5;
    std::uint8_t f62 = 6;
    std::uint8_t f63 = 0;
    std::uint8_t f64 = 1;
    std::uint8_t f65 = 2;
    std::uint8_t f66 = 3;
    std::uint8_t f67 = 4;
    std::uint8_t f68 = 5;
    std::uint8_t f69 = 6;
    std::uint8_t f70 = 0;
    std::uint8_t f71 = 1;
    std::uint8_t f72 = 2;
    std::uint8_t f73 = 3;
    std::uint8_t f74 = 4;
    std::uint8_t f75 = 5;
    std::uint8_t f76 = 6;
    std::uint8_t f77 = 0;
    std::uint8_t f78 = 1;
    std::uint8_t f79 = 2;
    std::uint8_t f80 = 3;
    std::uint8_t f81 = 4;
    std::uint8_t f82 = 5;
    std::uint8_t f83 = 6;
    std::uint8_t f84 = 0;
    std::uint8_t f85 = 1;
    std::uint8_t f86 = 2;
    std::uint8_t f87 = 3;
    std::uint8_t f88 = 4;
    std::uint8_t f89 = 5;
    std::uint8_t f90 = 6;
    std::uint8_t f91 = 0;
    std::uint8_t f92 = 1;
    std::uint8_t f93 = 2;
    std::uint8_t f94 = 3;
    std::uint8_t f95 = 4;
    std::uint8_t f96 = 5;
    std::uint8_t f97 = 6;
    std::uint8_t f98 = 0;
    std::uint8_t f99 = 1;
    std::uint8_t f100 = 2;
    std::uint8_t f101 = 3;
    std::uint8_t f102 = 4;
    std::uint8_t f103 = 5;
    std::uint8_t f104 = 6;
    std::uint8_t f105 = 0;
    std::uint8_t f106 = 1;
    std::uint8_t f107 = 2;
    std::uint8_t f108 = 3;
    std::uint8_t f109 = 4;
    std::uint8_t f110 = 5;
    std::uint8_t f111 = 6;
    std::uint8_t f112 = 0;
    std::uint8_t f113 = 1;
    std::uint8_t f114 = 2;
    std::uint8_t f115 = 3;
    std::uint8_t f116 = 4;
    std::uint8_t f117 = 5;
    std::uint8_t f118 = 6;
    std::uint8_t f119 = 0;
    std::uint8_t f120 = 1;
    std::uint8_t f121 = 2;
    std::uint8_t f122 = 3;
    std::uint8_t f123 = 4;
    std::uint8_t f124 = 5;
    std::uint8_t f125 = 6;
    std::uint8_t f126 = 0;
    std::uint8_t f127 = 1;
    std::uint8_t f128 = 2;
};
NNH_SAVE_TYPE(Wide, "wide128", NNH_SAVE_FIELD(f1, 1), NNH_SAVE_FIELD(f2, 2), NNH_SAVE_FIELD(f3, 3), NNH_SAVE_FIELD(f4, 4), NNH_SAVE_FIELD(f5, 5), NNH_SAVE_FIELD(f6, 6),
              NNH_SAVE_FIELD(f7, 7), NNH_SAVE_FIELD(f8, 8), NNH_SAVE_FIELD(f9, 9), NNH_SAVE_FIELD(f10, 10), NNH_SAVE_FIELD(f11, 11), NNH_SAVE_FIELD(f12, 12),
              NNH_SAVE_FIELD(f13, 13), NNH_SAVE_FIELD(f14, 14), NNH_SAVE_FIELD(f15, 15), NNH_SAVE_FIELD(f16, 16), NNH_SAVE_FIELD(f17, 17), NNH_SAVE_FIELD(f18, 18),
              NNH_SAVE_FIELD(f19, 19), NNH_SAVE_FIELD(f20, 20), NNH_SAVE_FIELD(f21, 21), NNH_SAVE_FIELD(f22, 22), NNH_SAVE_FIELD(f23, 23), NNH_SAVE_FIELD(f24, 24),
              NNH_SAVE_FIELD(f25, 25), NNH_SAVE_FIELD(f26, 26), NNH_SAVE_FIELD(f27, 27), NNH_SAVE_FIELD(f28, 28), NNH_SAVE_FIELD(f29, 29), NNH_SAVE_FIELD(f30, 30),
              NNH_SAVE_FIELD(f31, 31), NNH_SAVE_FIELD(f32, 32), NNH_SAVE_FIELD(f33, 33), NNH_SAVE_FIELD(f34, 34), NNH_SAVE_FIELD(f35, 35), NNH_SAVE_FIELD(f36, 36),
              NNH_SAVE_FIELD(f37, 37), NNH_SAVE_FIELD(f38, 38), NNH_SAVE_FIELD(f39, 39), NNH_SAVE_FIELD(f40, 40), NNH_SAVE_FIELD(f41, 41), NNH_SAVE_FIELD(f42, 42),
              NNH_SAVE_FIELD(f43, 43), NNH_SAVE_FIELD(f44, 44), NNH_SAVE_FIELD(f45, 45), NNH_SAVE_FIELD(f46, 46), NNH_SAVE_FIELD(f47, 47), NNH_SAVE_FIELD(f48, 48),
              NNH_SAVE_FIELD(f49, 49), NNH_SAVE_FIELD(f50, 50), NNH_SAVE_FIELD(f51, 51), NNH_SAVE_FIELD(f52, 52), NNH_SAVE_FIELD(f53, 53), NNH_SAVE_FIELD(f54, 54),
              NNH_SAVE_FIELD(f55, 55), NNH_SAVE_FIELD(f56, 56), NNH_SAVE_FIELD(f57, 57), NNH_SAVE_FIELD(f58, 58), NNH_SAVE_FIELD(f59, 59), NNH_SAVE_FIELD(f60, 60),
              NNH_SAVE_FIELD(f61, 61), NNH_SAVE_FIELD(f62, 62), NNH_SAVE_FIELD(f63, 63), NNH_SAVE_FIELD(f64, 64), NNH_SAVE_FIELD(f65, 65), NNH_SAVE_FIELD(f66, 66),
              NNH_SAVE_FIELD(f67, 67), NNH_SAVE_FIELD(f68, 68), NNH_SAVE_FIELD(f69, 69), NNH_SAVE_FIELD(f70, 70), NNH_SAVE_FIELD(f71, 71), NNH_SAVE_FIELD(f72, 72),
              NNH_SAVE_FIELD(f73, 73), NNH_SAVE_FIELD(f74, 74), NNH_SAVE_FIELD(f75, 75), NNH_SAVE_FIELD(f76, 76), NNH_SAVE_FIELD(f77, 77), NNH_SAVE_FIELD(f78, 78),
              NNH_SAVE_FIELD(f79, 79), NNH_SAVE_FIELD(f80, 80), NNH_SAVE_FIELD(f81, 81), NNH_SAVE_FIELD(f82, 82), NNH_SAVE_FIELD(f83, 83), NNH_SAVE_FIELD(f84, 84),
              NNH_SAVE_FIELD(f85, 85), NNH_SAVE_FIELD(f86, 86), NNH_SAVE_FIELD(f87, 87), NNH_SAVE_FIELD(f88, 88), NNH_SAVE_FIELD(f89, 89), NNH_SAVE_FIELD(f90, 90),
              NNH_SAVE_FIELD(f91, 91), NNH_SAVE_FIELD(f92, 92), NNH_SAVE_FIELD(f93, 93), NNH_SAVE_FIELD(f94, 94), NNH_SAVE_FIELD(f95, 95), NNH_SAVE_FIELD(f96, 96),
              NNH_SAVE_FIELD(f97, 97), NNH_SAVE_FIELD(f98, 98), NNH_SAVE_FIELD(f99, 99), NNH_SAVE_FIELD(f100, 100), NNH_SAVE_FIELD(f101, 101), NNH_SAVE_FIELD(f102, 102),
              NNH_SAVE_FIELD(f103, 103), NNH_SAVE_FIELD(f104, 104), NNH_SAVE_FIELD(f105, 105), NNH_SAVE_FIELD(f106, 106), NNH_SAVE_FIELD(f107, 107), NNH_SAVE_FIELD(f108, 108),
              NNH_SAVE_FIELD(f109, 109), NNH_SAVE_FIELD(f110, 110), NNH_SAVE_FIELD(f111, 111), NNH_SAVE_FIELD(f112, 112), NNH_SAVE_FIELD(f113, 113), NNH_SAVE_FIELD(f114, 114),
              NNH_SAVE_FIELD(f115, 115), NNH_SAVE_FIELD(f116, 116), NNH_SAVE_FIELD(f117, 117), NNH_SAVE_FIELD(f118, 118), NNH_SAVE_FIELD(f119, 119), NNH_SAVE_FIELD(f120, 120),
              NNH_SAVE_FIELD(f121, 121), NNH_SAVE_FIELD(f122, 122), NNH_SAVE_FIELD(f123, 123), NNH_SAVE_FIELD(f124, 124), NNH_SAVE_FIELD(f125, 125), NNH_SAVE_FIELD(f126, 126),
              NNH_SAVE_FIELD(f127, 127), NNH_SAVE_FIELD(f128, 128));

struct Row {
    std::uint16_t a = 1;
    std::uint8_t b = 2;
    nnh::data::String<4> c;
};
NNH_SAVE_STRUCT(Row, NNH_SAVE_FIELD(a, 1), NNH_SAVE_FIELD(b, 2), NNH_SAVE_FIELD(c, 3));
struct Rows {
    nnh::data::List<Row, 256> rows;
};
NNH_SAVE_TYPE(Rows, "rows", NNH_SAVE_FIELD(rows, 1));

} // namespace cost

namespace {

/// Values the decoder visits for `b` (an encoded save of T): applied or skipped, items counted once.
template <class T> std::uint32_t visited(const Bytes& b) {
    static T value;
    nnh::save::Report report;
    nnh::detail::SaveApply apply;
    apply.report = &report;
    const std::size_t header = 16 + std::string(nnh::detail::save_info_of<T>().key).size();
    nnh::detail::apply_fields(apply, nnh::detail::save_info_of<T>(), reinterpret_cast<std::uint8_t*>(&value),
                              b.data() + header, b.data() + b.size());
    return apply.visited;
}

void ignore_line(void*, const char*) {}

} // namespace

NNH_TEST(save_decoding_visits_each_field_of_a_128_field_struct_once) {
    const Bytes b = encoded(cost::Wide{});
    CHECK(visited<cost::Wide>(b) == 128);
    CHECK(b.size() == 16 + 7 + 127 * 3 + 4); // ids 1-127 one byte, 128 two bytes
}

NNH_TEST(save_decoding_visits_rows_linearly) {
    static cost::Rows rows;
    for (int i = 0; i < 256; ++i) rows.rows.push_back({static_cast<std::uint16_t>(i), 3, {}});
    const Bytes full = encoded(rows);
    CHECK(visited<cost::Rows>(full) == 1 + 256 + 256 * 3); // the field, its items, each item's three values
    rows.rows.clear();
    for (int i = 0; i < 16; ++i) rows.rows.push_back({});
    CHECK(visited<cost::Rows>(encoded(rows)) == 1 + 16 + 16 * 3);
}

NNH_TEST(save_calls_allocate_nothing) {
    fresh();
    static nnh::save::Buffer<mig::V3> buffer;
    mig::V1 v1;
    v1.coins = 10;
    v1.weapon = 4;
    CHECK(nnh::save::store("cost1.sav", v1, buffer.bytes, sizeof buffer.bytes).ok()); // the fake's files may allocate
    mig::V3 v3;
    std::string ignored;
    nnh::test::alloc_counter::reset();
    nnh::test::alloc_counter::enable(true);
    const auto n = nnh::save::encode(v3, buffer.bytes, sizeof buffer.bytes);
    const bool decoded = n.ok() && nnh::save::decode(buffer.bytes, n.value(), v3).ok();
    const auto info = nnh::save::info("cost1.sav");
    const bool exists = nnh::save::exists("cost1.sav");
    const bool dumped = nnh::save::dump(v3, buffer.bytes, sizeof buffer.bytes, ignore_line).ok() ||
                        nnh::build::flavor() == nnh::build::Flavor::Release;
    nnh::test::alloc_counter::enable(false);
    const int core_allocations = nnh::test::alloc_counter::count();
    CHECK(decoded && info.ok() && exists && dumped && core_allocations == 0);
    // a load (fs reads and the steps with their view) through the fake: the fake's reads don't allocate either
    nnh::test::alloc_counter::reset();
    nnh::test::alloc_counter::enable(true);
    const auto loaded = nnh::save::load("cost1.sav", v3, buffer);
    nnh::test::alloc_counter::enable(false);
    CHECK(loaded.ok() && loaded.value().migrated == 2 && nnh::test::alloc_counter::count() == 0);
    nnh::shutdown();
}
