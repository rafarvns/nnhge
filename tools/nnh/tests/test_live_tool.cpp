// The nnh side of live values (spec 030 T6.1, T6.5): the catalogue mirror, value text both ways with every refusal's
// exact text, expected values and comparisons, the --tunes reader with its file:line errors and limits, the steps it
// becomes, --tunes/--hold on the command line, --nnh-hold in the netloader's argv.

#include <cstring>
#include <string>
#include <vector>

#include "cli.hpp"
#include "live/live_kinds.hpp"
#include "nnh/detail/name_hash.hpp"
#include "netloader.hpp"
#include "remote/live_catalog.hpp"
#include "remote/live_text.hpp"
#include "remote/live_wire.hpp"
#include "remote/tunes_file.hpp"
#include "test.hpp"

using namespace nnh::tool::remote;
namespace wire = nnh::detail::remote;
namespace lv = nnh::detail::live;
using lv::Kind;

namespace {

void add(LiveCatalog& c, const char* name, Kind kind, std::uint8_t flags, std::uint32_t value, std::uint32_t min,
         std::uint32_t max, std::uint32_t step, const char* const* enums = nullptr, int enum_count = 0) {
    wire::LiveEntryHeader h;
    h.hash = nnh::detail::fnv1a32(name);
    h.kind = kind;
    h.flags = flags;
    h.value = value;
    h.def = value;
    h.min = min;
    h.max = max;
    h.step = step;
    std::uint8_t buf[wire::kRecordHeaderSize + wire::kLiveEntryMax];
    const std::size_t n = wire::encode_live_entry(h, name, enums, enum_count, buf);
    CHECK(c.add_entry(buf + wire::kRecordHeaderSize, static_cast<std::uint32_t>(n - wire::kRecordHeaderSize)));
}

constexpr const char* kShapes[] = {"circle", "square", "diamond"};

LiveCatalog sample() {
    LiveCatalog c;
    add(c, "player/jump", Kind::Float, wire::kEntryTunable, lv::float_bits(5.5f), lv::float_bits(0.0f),
        lv::float_bits(10.0f), lv::float_bits(0.25f));
    add(c, "spawn/count", Kind::Int32, wire::kEntryTunable, 6, 0, 32, 1);
    add(c, "debug/hitboxes", Kind::Bool, wire::kEntryTunable, 0, 0, 1, 0);
    add(c, "colors/sky", Kind::Color, wire::kEntryTunable, 0xFFFFFFFFu, 0, 0xFFFFFFFFu, 0);
    add(c, "spawn/shape", Kind::Enum, wire::kEntryTunable, 0, 0, 2, 0, kShapes, 3);
    add(c, "player/respawn", Kind::Action, wire::kEntryAction, 0, 0, 0, 0);
    add(c, "player/pos", Kind::Vec2, wire::kEntryWatch, 0, 0, 0, 0);
    add(c, "player/state", Kind::Text, wire::kEntryWatch, 0, 0, 0, 0);
    return c;
}

std::string set(const LiveCatalog& c, const char* name, const char* text, std::uint32_t& bits) {
    return parse_set_value(*c.find(name), text, bits);
}

wire::LiveValueRecord value(Kind k, const void* bytes, int n) {
    wire::LiveValueRecord v;
    v.kind = k;
    v.length = static_cast<std::uint8_t>(n);
    std::memcpy(v.value, bytes, static_cast<std::size_t>(n));
    return v;
}

} // namespace

NNH_TEST(live_catalog_keeps_names_in_order_and_groups) {
    LiveCatalog c = sample();
    CHECK(c.find("player/jump") != nullptr && c.find(nnh::detail::fnv1a32("spawn/count"))->name == "spawn/count");
    CHECK(c.list("player").size() == 4 && c.list().size() == 8 && c.list()[0]->name == "colors/sky");
    CHECK(c.is_group("player") && !c.is_group("player/jump") && !c.is_group("play"));
    CHECK(c.find("spawn/shape")->enum_names.size() == 3 && c.find("spawn/shape")->enum_names[2] == "diamond");
    c.remove(nnh::detail::fnv1a32("player/jump"));
    CHECK(c.find("player/jump") == nullptr);
    CHECK(!c.add_entry(reinterpret_cast<const std::uint8_t*>("short"), 5));
}

NNH_TEST(live_text_parses_every_kind_into_exact_bits) {
    const LiveCatalog c = sample();
    std::uint32_t b = 0;
    CHECK(set(c, "player/jump", "5", b).empty() && b == lv::float_bits(5.0f));
    CHECK(set(c, "player/jump", "-0", b).empty() && b == 0); // -0 sets +0 (review)
    CHECK(set(c, "player/jump", "1e-3", b).empty() && b == lv::float_bits(1e-3f));
    CHECK(set(c, "spawn/count", "+9", b).empty() && b == 9);
    CHECK(set(c, "debug/hitboxes", "true", b).empty() && b == 1);
    CHECK(set(c, "colors/sky", "#FFC040", b).empty() && b == lv::pack_rgba(0xFF, 0xC0, 0x40, 0xFF));
    CHECK(set(c, "colors/sky", "#ffc04080", b).empty() && b == lv::pack_rgba(0xFF, 0xC0, 0x40, 0x80));
    CHECK(set(c, "spawn/shape", "diamond", b).empty() && b == 2);
    CHECK(set(c, "spawn/shape", "1", b).empty() && b == 1);
}

NNH_TEST(live_text_refusals_have_their_exact_words) {
    const LiveCatalog c = sample();
    std::uint32_t b = 0;
    CHECK(set(c, "player/jump", "abc", b) == "player/jump is a float: \"abc\" is not a number");
    CHECK(set(c, "player/jump", "12", b) == "player/jump: 12 is outside 0..10");
    CHECK(set(c, "player/jump", "nan", b) == "player/jump: nan is not finite");
    CHECK(set(c, "spawn/count", "2.5", b) == "spawn/count is an int: \"2.5\" is not a whole number");
    CHECK(set(c, "spawn/count", "33", b) == "spawn/count: 33 is outside 0..32");
    CHECK(set(c, "debug/hitboxes", "yes", b) == "debug/hitboxes is a bool: use true or false");
    CHECK(set(c, "colors/sky", "#FFC0", b) == "colors/sky is a colour: use #RRGGBB or #RRGGBBAA");
    CHECK(set(c, "spawn/shape", "star", b) == "spawn/shape is an enum: \"star\" is not one of circle, square, diamond");
    CHECK(set(c, "player/respawn", "1", b) == "player/respawn is an action: run it with `action player/respawn`");
    CHECK(set(c, "player/pos", "1", b) == "player/pos is a watch: it can't be set");
    CHECK(unknown_name(c, "player/jmp") == "unknown name \"player/jmp\" (did you mean \"player/jump\"?)");
    CHECK(unknown_name(c, "zzzzzzzzzz") == "unknown name \"zzzzzzzzzz\"");
}

NNH_TEST(live_text_expected_values_and_comparisons) {
    const LiveCatalog c = sample();
    Expected e;
    CHECK(parse_expected(*c.find("player/jump"), "6.25", CompareOp::Eq, e).empty());
    const float f = 6.25f, g = 6.3f;
    CHECK(value_passes(value(Kind::Float, &f, 4), CompareOp::Eq, e, 0));
    CHECK(!value_passes(value(Kind::Float, &g, 4), CompareOp::Eq, e, 0));
    CHECK(value_passes(value(Kind::Float, &g, 4), CompareOp::Eq, e, 0.1));
    CHECK(value_passes(value(Kind::Float, &g, 4), CompareOp::Gt, e, 0));
    CHECK(parse_expected(*c.find("player/pos"), "(12.5, 40)", CompareOp::Eq, e).empty());
    const float xy[2] = {12.5f, 40.0f};
    CHECK(value_passes(value(Kind::Vec2, xy, 8), CompareOp::Eq, e, 0));
    CHECK(parse_expected(*c.find("player/pos"), "(1, 2)", CompareOp::Lt, e) == "player/pos: a Vec2 is compared with == or != only");
    CHECK(parse_expected(*c.find("player/state"), "idle", CompareOp::Eq, e).empty());
    CHECK(value_passes(value(Kind::Text, "idle", 4), CompareOp::Eq, e, 0));
    CHECK(!value_passes(value(Kind::Text, "idle", 4), CompareOp::Ne, e, 0));
    CHECK(parse_expected(*c.find("spawn/shape"), "square", CompareOp::Ge, e).empty());
    const std::uint32_t two = 2;
    CHECK(value_passes(value(Kind::Enum, &two, 4), CompareOp::Ge, e, 0));
    CHECK(value_text(value(Kind::Float, &f, 4), c.find("player/jump")) == "6.25");
    CHECK(value_text(value(Kind::Enum, &two, 4), c.find("spawn/shape")) == "diamond");
    CHECK(value_text(value(Kind::Vec2, xy, 8), nullptr) == "(12.5, 40)");
    CHECK(value_text(value(Kind::Text, "idle", 4), nullptr) == "\"idle\"");
}

NNH_TEST(tunes_file_reads_dumps_and_quoted_names) {
    const char* text = "# begin frame 42 (3 changed of 9)\n"
                       "speed = 1.5 # default 1\n"
                       "[colors]\n"
                       "sky = \"#FFC040FF\" # default \"#4678C8FF\"\n"
                       "[fx.spark]\n"
                       "big = 3\n"
                       "\"player/jump\" = 6.25\n";
    const TunesFile f = read_tunes(text, "t.toml");
    CHECK(f.errors.empty());
    CHECK(f.values.size() == 4);
    CHECK(f.values[0].name == "speed" && f.values[0].value == "1.5" && f.values[0].line == 2);
    CHECK(f.values[1].name == "colors/sky" && f.values[1].value == "#FFC040FF");
    CHECK(f.values[2].name == "fx/spark/big" && f.values[2].value == "3");
    CHECK(f.values[3].name == "player/jump" && f.values[3].line == 7);
}

NNH_TEST(tunes_file_errors_name_their_line) {
    const TunesFile f = read_tunes("a = 1\nnot a value\n\"fx//spark\" = 2\na = 3\n[x\n", "t.toml");
    CHECK(f.errors.size() == 4);
    CHECK(f.errors[0] == "t.toml:2: expected \"name = value\"");
    CHECK(f.errors[1] == "t.toml:3: name \"fx//spark\" has an empty part");
    CHECK(f.errors[2] == "t.toml:4: \"a\" is set twice (first at line 1)");
    CHECK(f.errors[3] == "t.toml:5: expected \"[group]\"");
    const std::string big(kMaxTunesBytes + 1, ' ');
    CHECK(read_tunes(big, "t.toml").errors.size() == 1);
}

NNH_TEST(tunes_file_mini_fuzz_never_crashes) {
    std::string seed = "[a.b]\nc = \"#FF\" # x\n\"d/e\" = 1\nf = true\n";
    std::uint32_t s = 7;
    for (int i = 0; i < 20000; ++i) {
        std::string m = seed;
        for (int k = 0; k < 3; ++k) {
            s = s * 1664525u + 1013904223u;
            m[(s >> 8) % m.size()] = static_cast<char>(s >> 24);
        }
        const TunesFile f = read_tunes(m, "f");
        CHECK(f.values.size() <= 4);
    }
}

NNH_TEST(tunes_file_becomes_set_steps_and_a_resume) {
    const TunesFile f = read_tunes("[player]\njump = 6.25\n", "keep.toml");
    const std::vector<Step> steps = tunes_steps(f, "keep.toml");
    CHECK(steps.size() == 2);
    CHECK(steps[0].command == Command::Set && steps[0].text == "player/jump" && steps[0].value == "6.25");
    CHECK(steps[0].origin == "keep.toml:2");
    CHECK(steps[1].command == Command::Resume);
}

NNH_TEST(cli_parses_tunes_and_hold) {
    nnh::tool::Options o;
    std::string e;
    CHECK(nnh::tool::parse_command_line({"run", "live", "--tunes", "t.toml"}, o, e));
    CHECK(o.tunes == "t.toml" && o.hold);
    CHECK(nnh::tool::parse_command_line({"run", "live", "--hold"}, o, e) && o.hold && o.tunes.empty());
    CHECK(!nnh::tool::parse_command_line({"run", "live", "--tunes"}, o, e) && e == "--tunes needs a file (a dump)");
    CHECK(!nnh::tool::parse_command_line({"build", "live", "--hold"}, o, e) && e == "--hold is for nnh run");
    CHECK(!nnh::tool::parse_command_line({"run", "live", "--hold", "--release"}, o, e));
    CHECK(std::string(nnh::tool::help_text()).find("--tunes <file>") != std::string::npos);
}

NNH_TEST(netloader_args_carry_the_hold) {
    const std::vector<std::uint8_t> a = nnh::tool::netloader_args({"live.3dsx", "--nnh-hold"});
    const std::string s(a.begin(), a.end());
    CHECK(s.find("--nnh-hold") != std::string::npos);
}

NNH_TEST(live_text_refuses_a_second_sign) {
    const LiveCatalog c = sample();
    std::uint32_t b = 0;
    CHECK(!set(c, "player/jump", "+-5", b).empty());
    CHECK(!set(c, "spawn/count", "+-5", b).empty());
    CHECK(set(c, "player/jump", "-0", b).empty() && b == 0); // -0 sets +0
}

NNH_TEST(live_text_integer_watches_check_their_range) {
    LiveCatalog c;
    struct Case {
        const char* name;
        Kind kind;
        const char* bad;
        const char* why;
    };
    const Case cases[] = {
        {"w/i8", Kind::Int8, "128", "w/i8: 128 is outside -128..127"},
        {"w/i16", Kind::Int16, "-32769", "w/i16: -32769 is outside -32768..32767"},
        {"w/i32", Kind::Int32, "2147483648", "w/i32: 2147483648 is outside -2147483648..2147483647"},
        {"w/u8", Kind::UInt8, "256", "w/u8: 256 is outside 0..255"},
        {"w/u16", Kind::UInt16, "-1", "w/u16: -1 is outside 0..65535"},
        {"w/u32", Kind::UInt32, "4294967296", "w/u32: 4294967296 is outside 0..4294967295"},
    };
    for (const Case& k : cases) add(c, k.name, k.kind, wire::kEntryWatch, 0, 0, 0, 0);
    for (const Case& k : cases) {
        Expected e;
        CHECK(parse_expected(*c.find(k.name), k.bad, CompareOp::Eq, e) == k.why);
        CHECK(parse_expected(*c.find(k.name), "1", CompareOp::Eq, e).empty());
    }
}
