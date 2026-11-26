// The dump (spec 030 T2.5, FR-14): the exact lines — ungrouped names first, a table per group path in byte order,
// colours, enums, bools, the default as a comment, nothing changed, actions left out.

#include <cstring>
#include <string>
#include <vector>

#include "live/live_dump.hpp"
#include "live/live_registry.hpp"
#include "live_fixture.hpp"
#include "nnh/gfx.hpp"

using namespace nnh::detail::live;
using live_test::restart;
namespace live = nnh::live;

namespace {

std::vector<std::string> g_lines;
void collect(void*, const char* line) { g_lines.emplace_back(line); }

std::vector<std::string> dump(std::uint32_t frame) {
    g_lines.clear();
    write_dump(*g_registry, frame, collect, nullptr);
    return g_lines;
}

enum class Shape : std::uint8_t { Circle, Square, Diamond };
constexpr const char* kShapes[] = {"circle", "square", "diamond"};

} // namespace

NNH_TEST(live_dump_nothing_changed) {
    if constexpr (!live::kEnabled) return;
    restart();
    float jump = 4.5f;
    live::Handle h = live::tune("player/jump", &jump, {.min = 0, .max = 10, .step = 0.25f});
    live::Handle a = live::action("player/respawn", [](void*) {});
    const std::vector<std::string> lines = dump(12);
    CHECK(lines.size() == 2);
    CHECK(lines[0] == "dump: begin frame 12 (0 changed of 1)");
    CHECK(lines[1] == "dump: end");
}

NNH_TEST(live_dump_lines_by_group_with_defaults) {
    if constexpr (!live::kEnabled) return;
    restart();
    float jump = 4.5f, gravity = 0.3f, speed = 1;
    std::int32_t count = 6;
    bool hitboxes = false;
    nnh::Color spark{0xFF, 0xFF, 0xFF, 0xFF};
    Shape shape = Shape::Circle;
    float top = 2, big = 1;
    live::Handle h1 = live::tune("player/jump", &jump, {.min = 0, .max = 10, .step = 0.25f});
    live::Handle h2 = live::tune("player/gravity", &gravity, {.min = 0, .max = 1, .step = 0.01f});
    live::Handle h3 = live::tune("speed", &speed, {.min = 0, .max = 4, .step = 0.5f});
    live::Handle h4 = live::tune("spawn/count", &count, {.min = 0, .max = 32, .step = 1});
    live::Handle h5 = live::tune("debug/hitboxes", &hitboxes);
    live::Handle h6 = live::tune("fx/spark", &spark);
    live::Handle h7 = live::tune("spawn/shape", &shape, kShapes);
    live::Handle h8 = live::tune("fx/spark2/big", &big, {.min = 0, .max = 4, .step = 1});
    live::Handle h9 = live::tune("top", &top, {.min = 0, .max = 4, .step = 1});
    jump = 5.25f;
    speed = 1.5f;
    count = 9;
    hitboxes = true;
    spark = {0xFF, 0xC0, 0x40, 0xFF};
    shape = Shape::Diamond;
    big = 3;
    const std::vector<std::string> lines = dump(230);
    const std::vector<std::string> expected = {
        "dump: begin frame 230 (7 changed of 9)",
        "dump: speed = 1.5 # default 1",
        "dump: [debug]",
        "dump: hitboxes = true # default false",
        "dump: [fx]",
        "dump: spark = \"#FFC040FF\" # default \"#FFFFFFFF\"",
        "dump: [fx.spark2]",
        "dump: big = 3 # default 1",
        "dump: [player]",
        "dump: jump = 5.25 # default 4.5",
        "dump: [spawn]",
        "dump: count = 9 # default 6",
        "dump: shape = \"diamond\" # default \"circle\"",
        "dump: end",
    };
    CHECK(lines.size() == expected.size());
    for (std::size_t i = 0; i < lines.size() && i < expected.size(); ++i) {
        if (lines[i] != expected[i]) std::printf("    line %d: \"%s\"\n", static_cast<int>(i), lines[i].c_str());
        CHECK(lines[i] == expected[i]);
    }
}

NNH_TEST(live_dump_keeps_one_table_per_group_when_names_interleave) {
    if constexpr (!live::kEnabled) return;
    restart();
    float a = 0, by = 0, c = 0;
    live::Handle ha = live::tune("g/a", &a, {.min = 0, .max = 4, .step = 1});
    live::Handle hb = live::tune("g/b/y", &by, {.min = 0, .max = 4, .step = 1});
    live::Handle hc = live::tune("g/c", &c, {.min = 0, .max = 4, .step = 1});
    a = by = c = 1;
    const std::vector<std::string> lines = dump(1);
    const std::vector<std::string> expected = {
        "dump: begin frame 1 (3 changed of 3)", "dump: [g]", "dump: a = 1 # default 0", "dump: c = 1 # default 0",
        "dump: [g.b]", "dump: y = 1 # default 0", "dump: end",
    };
    CHECK(lines == expected);
}
