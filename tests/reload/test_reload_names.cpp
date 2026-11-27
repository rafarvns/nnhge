// Load names, path matching and the log line's list (spec 031 T1.4, FR-13, FR-15).

#include <cstdio>
#include <cstring>

#include "reload/reload_names.hpp"
#include "test.hpp"

using namespace nnh::detail::reload;

NNH_TEST(reload_names_load_name_drops_the_last_extension) {
    CHECK(load_name_length("art/hero.png") == 8);
    CHECK(load_name_length("enemies/goblin.enemy") == 14);
    CHECK(load_name_length("hero.ase.json") == 8);       // "hero.ase"
    CHECK(load_name_length("levels/v1.2/room") == 16);   // a dot in a folder isn't an extension
    CHECK(load_name_length("README") == 6);
    CHECK(load_name_length(".nnh_assets") == 11);       // a leading dot: no extension
    CHECK(load_name_length("data/.nnh_data") == 14);
    CHECK(load_name_length("a/b.") == 3);               // an empty extension still goes
}

NNH_TEST(reload_names_matches_paths_and_load_names) {
    CHECK(matches("art/hero.png", "art/hero.png"));
    CHECK(matches("art/hero", "art/hero.png"));
    CHECK(!matches("art/her", "art/hero.png"));
    CHECK(!matches("art/hero.pn", "art/hero.png"));
    CHECK(!matches("art/hero.png.png", "art/hero.png"));
    CHECK(matches("hero.ase", "hero.ase.json"));
    CHECK(!matches("hero", "hero.ase.json"));
    CHECK(matches(".nnh_assets", ".nnh_assets"));
    CHECK(!matches("", ".nnh_assets"));
    CHECK(!matches(nullptr, "a.png"));
    CHECK(!matches("a", nullptr));
    CHECK(!matches("Art/hero", "art/hero.png")); // case-sensitive
}

NNH_TEST(reload_names_list_joins_load_names) {
    const char* paths[] = {"art/hero.png", "levels/level1.nnhmap", "enemies/goblin.enemy"};
    char out[kListLimit + 1];
    const std::size_t n = format_list(paths, 3, out, sizeof out);
    CHECK(std::strcmp(out, "art/hero, levels/level1, enemies/goblin") == 0);
    CHECK(n == std::strlen(out));
    CHECK(format_list(paths, 0, out, sizeof out) == 0);
    CHECK(out[0] == '\0');
}

NNH_TEST(reload_names_list_cut_at_every_length_around_the_limit) {
    // names of 10 characters ("nameNN.png" → "nameNN" + padding): ", " joins them
    static char storage[40][24];
    const char* paths[40];
    for (int i = 0; i < 40; ++i) {
        std::snprintf(storage[i], sizeof storage[i], "n%08d.png", i); // load name 9 chars
        paths[i] = storage[i];
    }
    char out[kListLimit + 1];
    for (int count = 1; count <= 40; ++count) {
        const std::size_t n = format_list(paths, count, out, sizeof out);
        CHECK(n <= kListLimit);
        CHECK(n == std::strlen(out));
        const std::size_t full = static_cast<std::size_t>(count) * 9 + static_cast<std::size_t>(count - 1) * 2;
        if (full <= kListLimit) {
            CHECK(n == full);
            CHECK(std::strstr(out, "...") == nullptr);
        } else {
            CHECK(std::strstr(out, ", ... (+") != nullptr);
        }
    }
    // 14 names fit in 152 characters; 15 need 163: the cut keeps 13 (141 + ", ... (+2)" = 151 ≤ 160; 14 would be 162)
    CHECK(format_list(paths, 15, out, sizeof out) == 151);
    char tail[16];
    std::snprintf(tail, sizeof tail, "%s", out + std::strlen(out) - 8);
    CHECK(std::strcmp(tail, "... (+2)") == 0);
}

NNH_TEST(reload_names_list_with_a_first_name_too_long) {
    static char longest[256];
    std::memset(longest, 'x', 200);
    std::strcpy(longest + 200, ".png");
    const char* paths[] = {longest, "b.png"};
    char out[kListLimit + 1];
    format_list(paths, 2, out, sizeof out);
    CHECK(std::strcmp(out, "... (+2)") == 0);
}

NNH_TEST(reload_names_a_short_buffer_never_overflows) {
    const char* paths[] = {"art/a_rather_long_name.png", "art/another_long_name.png", "levels/room_number_one.nnhmap"};
    char out[24];
    out[sizeof out - 1] = 'Z';
    const std::size_t n = format_list(paths, 3, out, 16); // shorter than the contract asks (review nit)
    CHECK(n < 16);
    CHECK(std::strlen(out) == n);
    CHECK(out[sizeof out - 1] == 'Z');
}
