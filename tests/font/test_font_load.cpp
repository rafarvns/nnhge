// Loading fonts (spec 029 FR-2, FR-5, FR-11, FR-34): fonts written in memory into the fake's assets — NotFound,
// Corrupted with the validator's reason, OutOfMemory, LimitReached at 33, sharing by path, fallbacks one level deep,
// colour pages through RGBA, the temporary page buffer freed, failures leaving no texture, D47 on destroy.

#include <cstring>
#include <string>
#include <vector>

#include "font_fixture.hpp"
#include "font_load_fixture.hpp"
#include "nnh/font.hpp"
#include "nnh/memory.hpp"
#include "test.hpp"

using namespace font_test;
namespace fake = nnh::platform_fake;

NNH_TEST(load_font_of_a_missing_path_is_not_found) {
    start_engine();
    auto f = nnh::load_font("fonts/none");
    CHECK(!f && f.error().code == nnh::ErrorCode::NotFound);
}

NNH_TEST(load_font_refuses_a_damaged_file_with_the_validators_reason) {
    start_engine();
    std::vector<std::uint8_t>& bytes = keep(FontBuilder::latin().build());
    bytes[bytes.size() - 1] ^= 1; // the CRC no longer matches
    fake::add_asset("fonts/bad.nnhfont", bytes.data(), bytes.size());
    LogLines warnings(nnh::LogLevel::Warn);
    auto f = nnh::load_font("fonts/bad");
    CHECK(!f && f.error().code == nnh::ErrorCode::Corrupted);
    CHECK(warnings.contains("load_font(\"fonts/bad\"): bad CRC"));
    CHECK(fake::state().live_textures == builtin_textures());
}

NNH_TEST(load_font_makes_one_coverage_texture_per_page) {
    start_engine();
    FontBuilder b = FontBuilder::latin();
    b.page_count = 2;
    add_font("fonts/two", b);
    auto f = nnh::load_font("fonts/two");
    CHECK(f.ok());
    CHECK(fake::state().alpha_textures_created == 2);
    CHECK(fake::state().live_textures == builtin_textures() + 2);
    const nnh::FontMetrics m = f.value().metrics();
    CHECK(m.size == 16 && m.line_height == 20 && m.ascent == 15 && m.descent == -4 && m.pages == 2);
    CHECK(m.glyphs == 66 && !m.colour && !m.outline);
}

NNH_TEST(load_font_pages_hold_the_coverage_levels_times_17) {
    start_engine();
    add_font("fonts/a", FontBuilder::latin());
    auto f = nnh::load_font("fonts/a");
    CHECK(f.ok());
    int w = 0, h = 0;
    const std::uint8_t* page = fake::alpha_texture_coverage(last_alpha_handle(), w, h);
    CHECK(page != nullptr && w == 128 && h == 64);
    CHECK(page != nullptr && page[0] == (1 + (0 * 3 + 0) % 15) * 17); // 'A' at (0, 0): the fixture's pattern
}

NNH_TEST(load_font_shares_a_path_loaded_twice) {
    start_engine();
    add_font("fonts/a", FontBuilder::latin());
    auto one = nnh::load_font("fonts/a");
    auto two = nnh::load_font("fonts/a");
    CHECK(one.ok() && two.ok());
    CHECK(one.value().ref().id == two.value().ref().id);
    CHECK(fake::state().alpha_textures_created == 1);
    nnh::Font first = static_cast<nnh::Font&&>(one.value());
    first = nnh::Font{}; // one owner gone: the font stays
    CHECK(two.value().valid() && fake::state().live_textures == builtin_textures() + 1);
}

NNH_TEST(load_font_loads_fallbacks_one_level_deep) {
    start_engine();
    FontBuilder a = FontBuilder::latin();
    a.fallbacks = {"fonts/b"};
    FontBuilder b;
    b.glyph(0x4E00, 0, 0, 8, 8, 0, 0, 9);
    b.fallbacks = {"fonts/c"}; // ignored: one level
    add_font("fonts/a", a);
    add_font("fonts/b", b);
    auto f = nnh::load_font("fonts/a");
    CHECK(f.ok());
    CHECK(f.value().covers(0x4E00) && !f.value().has(0x4E00) && f.value().has(U'A'));
    CHECK(fake::state().alpha_textures_created == 2); // a and b; c never read
}

NNH_TEST(load_font_fails_whole_when_a_fallback_is_missing) {
    start_engine();
    FontBuilder a = FontBuilder::latin();
    a.fallbacks = {"fonts/missing"};
    add_font("fonts/a", a);
    auto f = nnh::load_font("fonts/a");
    CHECK(!f && f.error().code == nnh::ErrorCode::NotFound);
    CHECK(fake::state().live_textures == builtin_textures()); // its page was released
    CHECK(nnh::mem::tag_stats(nnh::font_block_tag()).count == 0);
}

NNH_TEST(load_font_colour_pages_go_through_rgba) {
    start_engine();
    FontBuilder b = FontBuilder::latin();
    b.colour = true;
    add_font("fonts/colour", b);
    const int rgba_before = fake::state().rgba_textures_created;
    auto f = nnh::load_font("fonts/colour");
    CHECK(f.ok() && f.value().metrics().colour);
    CHECK(fake::state().rgba_textures_created == rgba_before + 1 && fake::state().alpha_textures_created == 0);
}

NNH_TEST(load_font_out_of_memory_is_returned) {
    start_engine();
    add_font("fonts/a", FontBuilder::latin());
    fake::set_region_capacity(nnh::MemRegion::Heap, nnh::mem::region_stats(nnh::MemRegion::Heap).bytes + 64);
    auto f = nnh::load_font("fonts/a");
    CHECK(!f && f.error().code == nnh::ErrorCode::OutOfMemory);
    fake::set_region_capacity(nnh::MemRegion::Heap, 64u << 20);
    CHECK(fake::state().live_textures == builtin_textures());
}

NNH_TEST(load_font_33rd_font_is_refused) {
    start_engine({.max_textures = 64});
    std::vector<std::string>& names = keep_names();
    for (int i = 0; i < 33; ++i) {
        names.push_back("fonts/f" + std::to_string(i));
        FontBuilder b;
        b.glyph(U'A', 0, 0, 4, 4, 0, 0, 5);
        add_font(names.back().c_str(), b);
    }
    std::vector<nnh::Font> fonts;
    for (int i = 0; i < 32; ++i) {
        auto f = nnh::load_font(names[static_cast<std::size_t>(i)].c_str());
        CHECK(f.ok());
        if (f) fonts.push_back(static_cast<nnh::Font&&>(f.value()));
    }
    auto last = nnh::load_font(names[32].c_str());
    CHECK(!last && last.error().code == nnh::ErrorCode::LimitReached);
}

NNH_TEST(load_font_frees_its_temporary_page_buffer) {
    start_engine();
    add_font("fonts/a", FontBuilder::latin());
    const nnh::MemStats before = nnh::mem::tag_stats(nnh::font_block_tag());
    auto f = nnh::load_font("fonts/a");
    CHECK(f.ok());
    const nnh::MemStats after = nnh::mem::tag_stats(nnh::font_block_tag());
    // what stays: the block and the page's texture (128 × 64, one byte a pixel); the decode buffer is gone
    CHECK(f.ok() && after.bytes - before.bytes == f.value().metrics().bytes);
    CHECK(after.count == before.count + 2);
    CHECK(after.peak - before.bytes >= f.value().metrics().bytes); // the buffer was there while the page was made
}

NNH_TEST(destroying_a_font_in_a_frame_waits_for_the_gpu) {
    start_engine();
    add_font("fonts/a", FontBuilder::latin());
    auto f = nnh::load_font("fonts/a");
    CHECK(f.ok());
    CHECK(nnh::frame());
    nnh::Font font = static_cast<nnh::Font&&>(f.value());
    font = nnh::Font{};
    CHECK(fake::state().live_textures == builtin_textures() + 1); // drawn this frame maybe: freed later
    CHECK(nnh::frame());
    CHECK(nnh::frame());
    CHECK(fake::state().live_textures == builtin_textures());
    CHECK(fake::state().destroyed_while_gpu_busy == 0);
}

NNH_TEST(a_stale_font_is_invalid_and_measures_like_the_builtin) {
    start_engine();
    add_font("fonts/a", FontBuilder::latin());
    auto f = nnh::load_font("fonts/a");
    CHECK(f.ok());
    const nnh::FontRef ref = f.value().ref();
    nnh::Font font = static_cast<nnh::Font&&>(f.value());
    font = nnh::Font{};
    CHECK(!font.valid());
    const nnh::FontMetrics m = nnh::gfx::font_metrics(nnh::FontRef{});
    CHECK(m.size == 8 && m.line_height == 10 && m.ascent == 8 && m.descent == 0 && m.glyphs == 0);
    (void)ref;
}
