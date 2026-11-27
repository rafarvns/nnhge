// The font kind of hot reload (spec 031 T5.4, FR-7, FR-16): the same Font and FontRef show the new metrics; the old
// page textures are released through D47 (the count of live textures is back after the GPU idle); a damaged file
// refuses the batch and keeps the old font.

#include "../reload/reload_fixture.hpp"
#include "font_fixture.hpp"
#include "font_load_fixture.hpp"
#include "nnh/font.hpp"

using namespace font_test;
using reload_test::Put;

namespace {

void start_font() {
    start_engine();
    add_font("fonts/body", FontBuilder::latin());
    reload_test::fake::remote_connect();
    reload_test::send(remote_test::hello());
    CHECK(nnh::frame());
}

} // namespace

NNH_TEST(font_reload_the_same_font_shows_new_metrics) {
    RELOAD_DEV_ONLY();
    start_font();
    {
        nnh::Font body = std::move(nnh::load_font("fonts/body").value());
        const nnh::FontRef ref = body.ref();
        CHECK(body.metrics().line_height == 20);
        const int live = reload_test::fake::state().live_textures;
        FontBuilder bigger = FontBuilder::latin();
        bigger.line_height = 26;
        bigger.page_count = 2;
        static std::vector<std::uint8_t> bytes;
        bytes = bigger.build();
        const Put batch[] = {Put{"fonts/body.nnhfont", bytes.data(), bytes.size()}};
        reload_test::send_batch(1, batch, 1);
        CHECK(nnh::frame());
        CHECK(nnh::assets::reload_count() == 1);
        CHECK(body.metrics().line_height == 26);
        CHECK(body.ref().id == ref.id);
        CHECK(nnh::frame()); // the old page was released at the reload point and freed at this GPU idle
        CHECK(reload_test::fake::state().live_textures == live + 1); // two pages now instead of one
    }
    nnh::shutdown();
}

NNH_TEST(font_reload_a_damaged_file_keeps_the_old_font) {
    RELOAD_DEV_ONLY();
    start_font();
    {
        nnh::Font body = std::move(nnh::load_font("fonts/body").value());
        static std::vector<std::uint8_t> bytes;
        bytes = FontBuilder::latin().build();
        bytes[bytes.size() - 1] ^= 1;
        const Put batch[] = {Put{"fonts/body.nnhfont", bytes.data(), bytes.size()}};
        reload_test::send_batch(1, batch, 1);
        CHECK(nnh::frame());
        CHECK(nnh::assets::reload_count() == 0);
        CHECK(body.metrics().line_height == 20);
    }
    nnh::shutdown();
}
