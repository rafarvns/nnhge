// Hot reload and tunables (spec 031 T6.2, FR-23): a reload never writes a tunable; when the game reloads data into a
// tuned field, the field shows the file's value; a value set in the panel and then overwritten by the file logs one
// warning; changed() stays the application point's; a tunable at its default overwritten by a reload is silent.

#include <cstdlib>

#include "../reload/reload_fixture.hpp"
#include "live/live_apply.hpp"
#include "live/live_registry.hpp"
#include "nnh/events.hpp"
#include "nnh/live.hpp"

namespace live = nnh::live;
using namespace nnh::detail::live;

namespace {

float g_speed = 4.5f;
float g_jump = 2.0f;

RemoteOp set_op(const char* name, std::uint32_t bits) {
    RemoteOp op;
    op.op = kOpSet;
    op.hash = nnh::detail::fnv1a32(name);
    op.bits = bits;
    return op;
}

/// The game: on AssetsReloaded it reads player/speed and player/jump from cfg/player.txt ("<speed> <jump>").
void read_config() {
    char text[32];
    if (reload_test::read_text("cfg/player.txt", text, sizeof text) < 0) return;
    char* end = nullptr;
    g_speed = std::strtof(text, &end);
    g_jump = std::strtof(end, nullptr);
}

void run_until_applied(std::uint32_t count) {
    for (int i = 0; i < 10 && nnh::assets::reload_count() < count; ++i) CHECK(nnh::frame());
}

} // namespace

NNH_TEST(live_reload_the_file_wins_and_an_edited_tunable_warns_once) {
    if constexpr (!live::kEnabled) return;
    RELOAD_DEV_ONLY();
    static const char kFirst[] = "4.5 2.0";
    const reload_test::Put assets[] = {reload_test::text("cfg/player.txt", kFirst)};
    char index[64];
    reload_test::start(assets, 1, index, sizeof index);
    g_speed = 4.5f;
    g_jump = 2.0f;
    {
        live::Handle speed = live::tune("player/speed", &g_speed, {.min = 0, .max = 10, .step = 0.25f});
        live::Handle jump = live::tune("player/jump", &g_jump, {.min = 0, .max = 10, .step = 0.25f});
        auto sub = nnh::events::subscribe<nnh::AssetsReloaded>([](const nnh::AssetsReloaded&) { read_config(); });
        CHECK(queue_op(*g_registry, set_op("player/speed", float_bits(5.25f)))); // the panel
        CHECK(nnh::frame());
        CHECK(g_speed == 5.25f && speed.changed());
        reload_test::Lines warned("set by a reload");
        const reload_test::Put batch[] = {reload_test::text("cfg/player.txt", "3.0 2.5")};
        reload_test::send_batch(1, batch, 1);
        run_until_applied(1);
        CHECK(nnh::assets::reload_count() == 1);
        CHECK(g_speed == 3.0f && g_jump == 2.5f); // the file wins; the reload itself wrote nothing
        CHECK(!speed.changed() && !jump.changed());
        CHECK(nnh::frame()); // the frame end of the batch's frame came first
        CHECK(warned.count == 1); // speed only: jump was at its default
        CHECK(std::strstr(warned.last, "live: player/speed was set by a reload (panel value 5.25, file value 3") !=
              nullptr);
    }
    nnh::shutdown();
}

NNH_TEST(live_reload_checks_at_most_16_edited_tunables) {
    if constexpr (!live::kEnabled) return;
    RELOAD_DEV_ONLY();
    static const char kFile[] = "x";
    const reload_test::Put assets[] = {reload_test::text("cfg/player.txt", kFile)};
    char index[64];
    reload_test::start(assets, 1, index, sizeof index);
    static float values[17];
    for (float& v : values) v = 1.0f;
    {
        const live::FloatRange range{.min = 0, .max = 10, .step = 0.5f};
        live::Handle handles[17] = {
            live::tune("group/value00", &values[0], range),
            live::tune("group/value01", &values[1], range),
            live::tune("group/value02", &values[2], range),
            live::tune("group/value03", &values[3], range),
            live::tune("group/value04", &values[4], range),
            live::tune("group/value05", &values[5], range),
            live::tune("group/value06", &values[6], range),
            live::tune("group/value07", &values[7], range),
            live::tune("group/value08", &values[8], range),
            live::tune("group/value09", &values[9], range),
            live::tune("group/value10", &values[10], range),
            live::tune("group/value11", &values[11], range),
            live::tune("group/value12", &values[12], range),
            live::tune("group/value13", &values[13], range),
            live::tune("group/value14", &values[14], range),
            live::tune("group/value15", &values[15], range),
            live::tune("group/value16", &values[16], range),
        };
        static const char* const kNames[17] = {
            "group/value00", "group/value01", "group/value02", "group/value03", "group/value04", "group/value05",
            "group/value06", "group/value07", "group/value08", "group/value09", "group/value10", "group/value11",
            "group/value12", "group/value13", "group/value14", "group/value15", "group/value16"};
        for (const char* name : kNames) CHECK(queue_op(*g_registry, set_op(name, float_bits(2.0f)))); // all edited
        CHECK(nnh::frame());
        auto sub = nnh::events::subscribe<nnh::AssetsReloaded>([](const nnh::AssetsReloaded&) {
            for (float& v : values) v = 3.0f; // the game's reload overwrote every one
        });
        reload_test::Lines warned("set by a reload");
        const reload_test::Put batch[] = {reload_test::text("cfg/player.txt", "y")};
        reload_test::send_batch(1, batch, 1);
        run_until_applied(1);
        CHECK(nnh::frame());
        CHECK(warned.count == 16); // one per noted tunable, 16 at most (a fixed table, no allocation)
    }
    nnh::shutdown();
}
