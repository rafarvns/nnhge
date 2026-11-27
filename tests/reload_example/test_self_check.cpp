// The hot reload example's self-check (examples/reload/self_check.hpp, spec 031 T7.3): the fold is pure over what the
// scene sees, every field moves the sum, equal snapshots give equal sums on every host, and the line is the one
// examples/reload/tests/check.nnhs expects.

#include <cstring>

#include "reload/self_check.hpp"
#include "test.hpp"

namespace {

ReloadSnapshot sample() {
    ReloadSnapshot s;
    s.backdrop_w = 128;
    s.backdrop_h = 128;
    s.hero_w = 32;
    s.hero_h = 32;
    s.goblin_hp = 30;
    s.goblin_speed = 1.25f;
    s.bat_hp = 12;
    s.bat_speed = 2.5f;
    s.map_tiles = 0x1234ABCDu;
    s.platform_x = 64;
    s.platform_y = 120;
    s.text_hash = 0xCAFEF00Du;
    s.text_width = 126;
    s.sound_ms = 250;
    s.gem_vertices = 24;
    s.reloads = 0;
    return s;
}

} // namespace

NNH_TEST(reload_self_check_equal_snapshots_equal_sums) {
    CHECK(reload_sum(sample()) == reload_sum(sample()));
    CHECK(reload_sum(sample()) == 0x5C9FEDE9u); // the same bits on every host compiler
}

NNH_TEST(reload_self_check_every_field_moves_the_sum) {
    const std::uint32_t base = reload_sum(sample());
    int moved = 0;
    for (int field = 0; field < kReloadSnapshotFields; ++field) {
        ReloadSnapshot s = sample();
        nudge(s, field);
        moved += reload_sum(s) != base ? 1 : 0;
    }
    CHECK(moved == kReloadSnapshotFields);
}

NNH_TEST(reload_self_check_line) {
    char line[64];
    format_reload_line(line, sizeof line, 0x0BADF00Du, 3);
    CHECK(std::strcmp(line, "reload: sum 0BADF00D after 3") == 0);
}
