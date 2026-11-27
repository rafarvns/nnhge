#pragma once
// Sprite library files for the hot reload tests (spec 031 T5.1): code-built libraries written as `.nnhsprite` bytes, a
// game started with one of them as art/hero.nnhsprite, and a batch that replaces it.

#include <vector>

#include "../reload/reload_fixture.hpp"
#include "library_fixture.hpp"
#include "sprite_file.hpp"

namespace sprite_reload_test {

using File = std::vector<std::uint8_t>;

/// 4 frames; clips "idle" (2 frames) and "walk" (3 frames), 4 updates per frame; with `states`: "rest" (idle) and "go"
/// (walk) and one rule.
inline File version1(bool states = false) {
    nnh::SpriteLibrary lib = anim_test::strip(4);
    const nnh::SpriteClipId idle = anim_test::clip(lib, "idle", 2, 4);
    const nnh::SpriteClipId walk = anim_test::clip(lib, "walk", 3, 4);
    if (states) {
        auto rest = lib.add_state("rest", idle);
        auto go = lib.add_state("go", walk);
        CHECK(rest.ok() && go.ok());
        CHECK(lib.add_transition({.from = rest.value(), .to = go.value(), .when = nnh::SpriteSwitch::AtEnd}).ok());
    }
    return anim_test::sprite_file(lib);
}

/// Bigger: 6 frames; "jump" first, then "idle" (2) and "walk" (4 frames); the same states (by clip name) when asked,
/// with another rule when `other_rule`.
inline File version2(bool states = false, bool other_rule = false) {
    nnh::SpriteLibrary lib = anim_test::strip(6);
    anim_test::clip(lib, "jump", 1, 4);
    const nnh::SpriteClipId idle = anim_test::clip(lib, "idle", 2, 4);
    const nnh::SpriteClipId walk = anim_test::clip(lib, "walk", 4, 4);
    if (states) {
        auto rest = lib.add_state("rest", idle);
        auto go = lib.add_state("go", walk);
        CHECK(rest.ok() && go.ok());
        CHECK(lib.add_transition({.from = rest.value(), .to = go.value(),
                                  .when = other_rule ? nnh::SpriteSwitch::Now : nnh::SpriteSwitch::AtEnd})
                  .ok());
    }
    return anim_test::sprite_file(lib);
}

/// Only "idle" (1 frame): "walk" is gone.
inline File without_walk() {
    nnh::SpriteLibrary lib = anim_test::strip(2);
    anim_test::clip(lib, "idle", 1, 4);
    return anim_test::sprite_file(lib);
}

inline File g_first;
inline char g_index[64];

/// The engine with art/hero.nnhsprite = `first`, attached to the in-memory PC.
inline void start(File first) {
    g_first = std::move(first);
    const reload_test::Put assets[] = {reload_test::Put{"art/hero.nnhsprite", g_first.data(), g_first.size()}};
    reload_test::start(assets, 1, g_index, sizeof g_index);
}

/// Replaces art/hero.nnhsprite with `file` (batch `id`) and runs frames until it is applied or refused.
inline void reload(const File& file, std::uint16_t id) {
    const reload_test::Put batch[] = {reload_test::Put{"art/hero.nnhsprite", file.data(), file.size()}};
    const std::uint32_t applied = nnh::assets::reload_count();
    const int refused = reload_test::records().refused;
    reload_test::send_batch(id, batch, 1);
    for (int i = 0; i < 10 && nnh::assets::reload_count() == applied && reload_test::records().refused == refused; ++i) {
        CHECK(nnh::frame());
    }
}

} // namespace sprite_reload_test
