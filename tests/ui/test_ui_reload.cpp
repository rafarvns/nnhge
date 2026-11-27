// The UI follows a hot reload (spec 031 T4.3, FR-7, FR-10): in the frame a batch is applied, a UI's update resolves its
// keys and measures its texts again (a reloaded string table, font or texture shows without game code); other frames
// leave the layout alone.

#include "../reload/reload_fixture.hpp"
#include "ui_fixture.hpp"

using reload_test::Put;

namespace {

const char* g_text = "Hi";
const char kA[] = "a";

} // namespace

NNH_TEST(ui_reload_a_batch_frame_resolves_and_measures_again) {
    RELOAD_DEV_ONLY();
    char index[64];
    const Put assets[] = {reload_test::text("a.txt", kA)};
    reload_test::start(assets, 1, index, sizeof index);
    {
        nnh::Ui ui = ui_test::make();
        ui.set_resolver([](std::uint32_t) { return g_text; });
        const nnh::UiWidget label = ui.label(ui.root(nnh::Screen::Top), {.text = nnh::UiText::key(1u)});
        ui_test::settle(ui);
        CHECK(ui.rect(label).w == 16);
        g_text = "Hello"; // what a reloaded string table would now answer
        CHECK(nnh::frame());
        ui_test::settle(ui);
        CHECK(ui.rect(label).w == 16); // no batch: the resolved text is kept
        const Put batch[] = {reload_test::text("a.txt", "b")};
        reload_test::send_batch(1, batch, 1);
        CHECK(nnh::frame());
        CHECK(nnh::assets::reload_count() == 1);
        ui_test::settle(ui);
        CHECK(ui.rect(label).w == 40);
    }
    g_text = "Hi";
    nnh::shutdown();
}
