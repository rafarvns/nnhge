// The Release forms (spec 030 T2.2, FR-20): every call compiles; in Release nothing is registered, changed() is false,
// variables keep their values and nothing allocates. Runs in every flavour; the Release checks only in pc-release.

#include "alloc_counter.hpp"
#include "live/live_block.hpp"
#include "live_fixture.hpp"
#include "nnh/gfx.hpp"
#include "nnh/memory.hpp"

namespace live = nnh::live;

namespace {

enum class Shape : std::uint8_t { Circle, Square };
constexpr const char* kShapes[] = {"circle", "square"};
void act(void*) {}

} // namespace

NNH_TEST(live_release_every_call_compiles_and_release_does_nothing) {
    live_test::restart();
    float f = 1.5f;
    std::int32_t i = 3;
    bool b = true;
    nnh::Color c{1, 2, 3, 4};
    Shape s = Shape::Square;
    const char* state = "idle";
    char text[8] = "abc";
    nnh::Vec2 pos{1, 2};
    const std::uint64_t news = alloc_counter::news();
    const int allocations = nnh::mem::live_allocations();
    {
        live::Handle h1 = live::tune("r/f", &f, {.min = 0, .max = 2, .step = 0.5f});
        live::Handle h2 = live::tune("r/i", &i, {.min = 0, .max = 9, .step = 1});
        live::Handle h3 = live::tune("r/b", &b);
        live::Handle h4 = live::tune("r/c", &c);
        live::Handle h5 = live::tune("r/s", &s, kShapes);
        live::Handle h6 = live::action("r/a", act);
        live::Handle h7 = live::watch("w/pos", &pos);
        live::Handle h8 = live::watch("w/state", &state);
        live::Handle h9 = live::watch("w/text", text);
        live::show("w/n", 5);
        live::show("w/t", "x");
        live::show_panel(live::Page::Stats);
        if constexpr (!live::kEnabled) {
            CHECK(!h1.valid() && !h1.changed() && h1.id() == 0);
            h1.reset();
            live::reset_all();
            live::reset_group("r");
            live::configure({});
            live::pause();
            CHECK(nnh::detail::live::g_registry == nullptr);
            CHECK(nnh::mem::live_allocations() == allocations);
        } else {
            CHECK(h1.valid() && h9.valid());
        }
    }
    CHECK(f == 1.5f && i == 3 && b && s == Shape::Square);
    CHECK(alloc_counter::news() == news);
    CHECK(sizeof(live::Handle) == 4);
}
