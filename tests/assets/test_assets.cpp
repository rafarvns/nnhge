// Reading the game's assets (nnh/assets.hpp, spec 007 FR-5, FR-6, FR-7, FR-8), through the fake platform.

#include <cstring>
#include <utility>

#include "nnh/app.hpp"
#include "nnh/assets.hpp"
#include "assets/assets_internal.hpp"
#include "nnh/memory.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;
namespace assets = nnh::assets;
using nnh::ErrorCode;

namespace {

constexpr char kIndex[] = "levels/level1.txt\nart/Hero.png\nempty.bin\n";
constexpr char kLevel[] = "platforms 6 8\n";

/// A fresh engine whose game has three assets (plus the index the build writes).
void start_with_assets() {
    nnh::shutdown();
    fake::reset();
    fake::add_asset(".nnh_assets", kIndex, sizeof kIndex - 1);
    fake::add_asset("levels/level1.txt", kLevel, sizeof kLevel - 1);
    fake::add_asset("art/Hero.png", "PNG?", 4);
    fake::add_asset("empty.bin", "", 0);
    CHECK(nnh::init());
}

bool last_log_has(const char* text) { return std::strstr(fake::state().last_console_line, text) != nullptr; }

} // namespace

NNH_TEST(assets_listed_in_the_index_exist) {
    start_with_assets();
    CHECK(assets::exists("levels/level1.txt"));
    CHECK(assets::exists("empty.bin"));
    CHECK(!assets::exists("levels/level2.txt"));
    CHECK(!assets::exists(".nnh_assets")); // the index itself is not an asset
    nnh::shutdown();
}

NNH_TEST(an_asset_is_read_into_the_callers_buffer) {
    start_with_assets();
    auto size = assets::size("levels/level1.txt");
    CHECK(size.ok() && size.value() == sizeof kLevel - 1);
    char buffer[64] = {};
    auto read = assets::read("levels/level1.txt", buffer, sizeof buffer);
    CHECK(read.ok() && read.value() == sizeof kLevel - 1);
    CHECK(std::memcmp(buffer, kLevel, sizeof kLevel - 1) == 0);
    nnh::shutdown();
}

NNH_TEST(a_buffer_too_small_is_refused_with_the_size_needed) {
    start_with_assets();
    char tiny[4];
    auto read = assets::read("levels/level1.txt", tiny, sizeof tiny);
    CHECK(!read.ok() && read.error().code == ErrorCode::LimitReached);
    CHECK(std::strstr(read.error().message, "14") != nullptr);
    nnh::shutdown();
}

NNH_TEST(load_gives_an_owned_buffer_counted_under_assets) {
    start_with_assets();
    const std::size_t before = nnh::mem::tag_stats(nnh::detail::assets_memory_tag()).count; // the index
    {
        auto loaded = assets::load("levels/level1.txt");
        CHECK(loaded.ok());
        nnh::AssetData data = std::move(loaded.value());
        CHECK(data.valid() && data.size() == sizeof kLevel - 1);
        CHECK(std::memcmp(data.data(), kLevel, data.size()) == 0);
        nnh::AssetData moved = std::move(data);
        CHECK(!data.valid() && moved.valid());
        CHECK(nnh::mem::tag_stats(nnh::detail::assets_memory_tag()).count == before + 1);
    }
    // Released with the object: only the index stays.
    CHECK(nnh::mem::tag_stats(nnh::detail::assets_memory_tag()).count == before);
    nnh::shutdown();
}

NNH_TEST(an_empty_asset_loads_as_an_empty_buffer) {
    start_with_assets();
    auto loaded = assets::load("empty.bin");
    CHECK(loaded.ok() && loaded.value().valid() && loaded.value().size() == 0);
    nnh::shutdown();
}

NNH_TEST(a_path_with_the_wrong_letter_case_is_not_found_and_the_log_suggests_the_right_one) {
    start_with_assets();
    CHECK(!assets::exists("art/hero.png"));
    auto size = assets::size("art/hero.png");
    CHECK(!size.ok() && size.error().code == ErrorCode::NotFound);
    CHECK(last_log_has("art/Hero.png"));
    nnh::shutdown();
}

NNH_TEST(invalid_paths_are_refused_without_touching_the_platform) {
    start_with_assets();
    const int reads = fake::state().asset_reads;
    char buffer[8];
    for (const char* bad : {"../secret", "/abs", "a\\b", "", "art//x"}) {
        auto read = assets::read(bad, buffer, sizeof buffer);
        CHECK(!read.ok() && read.error().code == ErrorCode::InvalidArgument);
    }
    CHECK(!assets::read(nullptr, buffer, sizeof buffer).ok());
    CHECK(fake::state().asset_reads == reads);
    nnh::shutdown();
}

NNH_TEST(an_asset_in_the_index_but_unreadable_is_an_io_error) {
    nnh::shutdown();
    fake::reset();
    const char index[] = "ghost.txt\n";
    fake::add_asset(".nnh_assets", index, sizeof index - 1);
    CHECK(nnh::init());
    auto size = assets::size("ghost.txt");
    CHECK(!size.ok() && size.error().code == ErrorCode::IoError);
    nnh::shutdown();
}

NNH_TEST(a_game_without_assets_finds_nothing) {
    nnh::shutdown();
    fake::reset();
    CHECK(nnh::init());
    CHECK(!assets::exists("levels/level1.txt"));
    auto size = assets::size("levels/level1.txt");
    CHECK(!size.ok() && size.error().code == ErrorCode::NotFound);
    nnh::shutdown();
}

NNH_TEST(an_oversized_index_is_ignored_with_a_warning) {
    nnh::shutdown();
    fake::reset();
    // 35 000 valid lines (70 KB): only the size limit can refuse it.
    static char huge[70 * 1024];
    for (std::size_t i = 0; i + 1 < sizeof huge; i += 2) {
        huge[i] = 'a';
        huge[i + 1] = '_';
    }
    for (std::size_t i = 1; i < sizeof huge; i += 2) huge[i] = static_cast<char>(10); // newline
    fake::add_asset(".nnh_assets", huge, sizeof huge);
    fake::add_asset("a", "x", 1);
    CHECK(nnh::init()); // the game still starts
    CHECK(!assets::exists("a"));
    nnh::shutdown();
}

NNH_TEST(assets_before_init_are_unsupported) {
    nnh::shutdown();
    auto size = assets::size("levels/level1.txt");
    CHECK(!size.ok() && size.error().code == ErrorCode::Unsupported);
}

NNH_TEST(assets_are_released_at_shutdown_and_reread_at_init) {
    start_with_assets();
    nnh::shutdown();
    CHECK(nnh::mem::tag_stats(nnh::detail::assets_memory_tag()).count == 0);
    fake::reset();
    const char index[] = "other.txt\n";
    fake::add_asset(".nnh_assets", index, sizeof index - 1);
    fake::add_asset("other.txt", "x", 1);
    CHECK(nnh::init());
    CHECK(assets::exists("other.txt") && !assets::exists("levels/level1.txt"));
    nnh::shutdown();
}
