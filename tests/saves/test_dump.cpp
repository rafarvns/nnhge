// The text dump (spec 027 T5.4, FR-17): the exact text of a small value (names, ids, kinds, nested indentation,
// sequence counts, escapes, floats with their bits, enum names), unknown and retired ids, a dump without the schema,
// `<path>.txt` written by a store when NNH_SAVE_DUMP is on (the test override), and Unsupported in Release.

#include <cstdio>
#include <string>

#include "migration_types.hpp"
#include "nnh/fs.hpp"
#include "saves/save_internal.hpp"
#include "saves_fixture.hpp"

using saves_test::Bytes;
using saves_test::encoded;
using saves_test::fresh;

namespace dumped {

struct Inner {
    std::uint8_t hp = 3;
};
NNH_SAVE_STRUCT(Inner, NNH_SAVE_FIELD(hp, 1));

struct Small {
    nnh::data::String<8> name;
    Inner inner;
    nnh::data::List<std::uint16_t, 4> list;
    float speed = 1.5f;
    bool flags[2] = {true, false};
    nnh::data::List<Inner, 2> rows;
    test_data::Element element = test_data::Element::Fire;
};
NNH_SAVE_TYPE(Small, "small", NNH_SAVE_FIELD(name, 1), NNH_SAVE_FIELD(inner, 2), NNH_SAVE_FIELD(list, 3),
              NNH_SAVE_FIELD(speed, 4), NNH_SAVE_FIELD(flags, 5), NNH_SAVE_FIELD(rows, 6), NNH_SAVE_FIELD(element, 7),
              NNH_SAVE_RETIRED(9));

struct Long {
    nnh::data::List<nnh::data::String<60>, 100> lines;
};
NNH_SAVE_TYPE(Long, "long_dump", NNH_SAVE_FIELD(lines, 1));

Small sample() {
    Small s;
    s.name.assign("Q\"\\\xC3\xA9");
    s.list.push_back(1);
    s.list.push_back(2);
    s.rows.push_back({7});
    return s;
}

} // namespace dumped

namespace {

void collect(void* context, const char* line) {
    std::string& text = *static_cast<std::string*>(context);
    text += line;
    text += '\n';
}

std::string first_line(const char* key, std::uint32_t version, std::size_t size) {
    char line[64];
    std::snprintf(line, sizeof line, "save '%s' v%u (%u bytes)\n", key, static_cast<unsigned>(version),
                  static_cast<unsigned>(size));
    return line;
}

} // namespace

#if !defined(NNH_FLAVOR_RELEASE)

NNH_TEST(save_dump_writes_one_line_per_field) {
    const dumped::Small s = dumped::sample();
    std::uint8_t scratch[256];
    std::string text;
    CHECK(nnh::save::dump(s, scratch, sizeof scratch, collect, &text).ok());
    const std::string expected = first_line("small", 1, encoded(s).size()) +
                                 "name (1) string = \"Q\\\"\\\\\\xC3\\xA9\"\n"
                                 "inner (2) struct\n"
                                 "  hp (1) u8 = 3\n"
                                 "list (3) u16 list[2]\n"
                                 "  [0] 1 2\n"
                                 "speed (4) f32 = 1.5 [3FC00000]\n"
                                 "flags (5) bool[2]\n"
                                 "  [0] true false\n"
                                 "rows (6) struct list[1]\n"
                                 "  [0]\n"
                                 "    hp (1) u8 = 7\n"
                                 "element (7) u8 = 5 (Fire)\n";
    CHECK(text == expected);
    if (text != expected) std::printf("%s", text.c_str());
}

NNH_TEST(save_dump_names_unknown_and_retired_ids) {
    mig::V1 v1;
    v1.coins = 1234;
    v1.weapon = 7;
    const Bytes b = encoded(v1);
    std::string text;
    CHECK(nnh::save::dump_bytes<mig::V3>(b.data(), b.size(), collect, &text).ok());
    CHECK(text == first_line("mig", 1, b.size()) + "#1 (retired) u32 = 1234\n#2 (retired) u16 = 7\n");
    mig::V3 v3;
    v3.gems = 5;
    const Bytes b3 = encoded(v3);
    text.clear();
    CHECK(nnh::save::dump_bytes<mig::V1>(b3.data(), b3.size(), collect, &text).ok());
    CHECK(text == first_line("mig", 3, b3.size()) + "#3 u16 list[0]\n#4 u32 = 5\n#5 u16 = 0\n");
}

NNH_TEST(save_dump_without_a_schema_names_ids_only) {
    const dumped::Small s = dumped::sample();
    const Bytes b = encoded(s);
    std::string text;
    CHECK(nnh::save::dump_bytes(b.data(), b.size(), collect, &text).ok());
    CHECK(text.find("#2 struct\n  #1 u8 = 3\n") != std::string::npos);
    CHECK(text.find("#7 u8 = 5\n") != std::string::npos); // no enum name without the schema
    std::string other;
    CHECK(nnh::save::dump_bytes<mig::V3>(b.data(), b.size(), collect, &other).ok()); // another key: ids only
    CHECK(other == text);
}

NNH_TEST(save_dump_refuses_a_damaged_buffer) {
    Bytes b = encoded(dumped::sample());
    b.resize(b.size() - 1);
    std::string text;
    CHECK(nnh::save::dump_bytes(b.data(), b.size(), collect, &text).error().code == nnh::ErrorCode::Corrupted);
    CHECK(text.empty());
}

NNH_TEST(save_dump_reads_a_file_and_falls_back_to_its_backup) {
    fresh();
    nnh::save::Buffer<dumped::Small> buffer;
    CHECK(nnh::save::store("dump.sav", dumped::sample(), buffer).ok());
    std::string text;
    CHECK(nnh::save::dump_file<dumped::Small>("dump.sav", buffer.bytes, sizeof buffer.bytes, collect, &text).ok());
    CHECK(text.find("element (7) u8 = 5 (Fire)") != std::string::npos);
    CHECK(nnh::fs::remove("dump.sav").ok());
    CHECK(nnh::save::dump_file<dumped::Small>("dump.sav", buffer.bytes, sizeof buffer.bytes, collect, &text)
              .error()
              .code == nnh::ErrorCode::NotFound);
    nnh::shutdown();
}

NNH_TEST(save_store_writes_the_dump_beside_the_save_when_asked) {
    fresh();
    nnh::detail::save_set_dump_override(1);
    nnh::save::Buffer<dumped::Small> buffer;
    CHECK(nnh::save::store("slot1.sav", dumped::sample(), buffer).ok());
    nnh::detail::save_set_dump_override(0);
    CHECK(nnh::save::store("slot2.sav", dumped::sample(), buffer).ok());
    nnh::detail::save_set_dump_override(-1);
    char file[1024] = {};
    const auto r = nnh::fs::read_file("slot1.sav.txt", file, sizeof file - 1);
    std::string text;
    CHECK(nnh::save::dump(dumped::sample(), buffer.bytes, sizeof buffer.bytes, collect, &text).ok());
    CHECK(r.ok() && std::string(file) == text);
    CHECK(!nnh::fs::exists("slot2.sav.txt"));
    nnh::shutdown();
}

NNH_TEST(save_store_writes_a_long_dump_in_pieces) {
    fresh();
    static dumped::Long value;
    nnh::data::String<60> line;
    line.assign("0123456789012345678901234567890123456789012345678901234567890");
    while (value.lines.push_back(line)) {}
    static nnh::save::Buffer<dumped::Long> buffer;
    nnh::detail::save_set_dump_override(1);
    nnh::platform_fake::clear_fs_log();
    CHECK(nnh::save::store("long.sav", value, buffer).ok());
    nnh::detail::save_set_dump_override(-1);
    const std::string calls = nnh::platform_fake::fs_log();
    CHECK(calls.find("WA") != std::string::npos); // the dump: written, then appended in 4 KB pieces
    std::string text;
    CHECK(nnh::save::dump(value, buffer.bytes, sizeof buffer.bytes, collect, &text).ok());
    static char file[16384];
    const auto r = nnh::fs::read_file("long.sav.txt", file, sizeof file - 1);
    CHECK(text.size() > 4096 && r.ok() && r.value() == text.size() && std::string(file, r.value()) == text);
    nnh::shutdown();
}

#else

NNH_TEST(save_dump_is_unsupported_in_release) {
    std::uint8_t scratch[256];
    std::string text;
    CHECK(nnh::save::dump(dumped::sample(), scratch, sizeof scratch, collect, &text).error().code ==
          nnh::ErrorCode::Unsupported);
    const Bytes b = encoded(dumped::sample());
    CHECK(nnh::save::dump_bytes(b.data(), b.size(), collect, &text).error().code == nnh::ErrorCode::Unsupported);
    CHECK(text.empty());
}

#endif
