// The frame's recorded 2D draws and how a pass is built from them (gfx/draw_list.hpp, spec 004
// FR-6, FR-7, FR-8, FR-13).

#include "gfx/draw_list.hpp"
#include "gfx/texture_table.hpp"
#include "test.hpp"

using nnh::Screen;
using nnh::Vec2;
using nnh::detail::DrawCommand;
using nnh::detail::DrawList;
using nnh::detail::TextureData;
using nnh::detail::TextureSlot;
using nnh::detail::TextureTable;
using Item = nnh::platform::DrawItem2D;
using Kind = nnh::platform::DrawItem2D::Kind;

namespace {

constexpr int kCapacity = 8;
DrawCommand g_storage[kCapacity];
std::uint16_t g_order[kCapacity];
TextureSlot g_slots[4];
Item g_items[kCapacity];

struct Fixture {
    DrawList list;
    TextureTable textures;
    int tex_slot = -1;
    std::uint32_t tex_gen = 0;

    Fixture() {
        list.reset(g_storage, g_order, kCapacity);
        textures.reset(g_slots, 4);
        tex_slot = textures.add(TextureData{42, 64, 32, 64 * 32 * 4}, tex_gen);
    }

    DrawCommand sprite(Vec2 position, int layer = 0) const {
        DrawCommand c;
        c.kind = Kind::Sprite;
        c.screen = Screen::Top;
        c.layer = static_cast<std::uint8_t>(layer);
        c.texture_slot = tex_slot;
        c.texture_generation = tex_gen;
        c.position = position;
        return c;
    }

    DrawCommand fill(Vec2 position, Vec2 size, int layer = 0, Screen screen = Screen::Top) const {
        DrawCommand c;
        c.kind = Kind::FillRect;
        c.screen = screen;
        c.layer = static_cast<std::uint8_t>(layer);
        c.position = position;
        c.second = size;
        return c;
    }

    DrawList::BuildResult build(Screen screen = Screen::Top, Vec2 camera = {}) const {
        return list.build(screen, camera, textures, g_items, kCapacity);
    }
};

} // namespace

NNH_TEST(draw_list_records_up_to_its_capacity_then_drops_and_counts) {
    Fixture f;
    for (int i = 0; i < kCapacity; ++i) CHECK(f.list.record(f.fill({0, 0}, {1, 1})));
    CHECK(!f.list.record(f.fill({0, 0}, {1, 1})));
    CHECK(!f.list.record(f.fill({0, 0}, {1, 1})));
    CHECK(f.list.count() == kCapacity && f.list.dropped() == 2);
}

NNH_TEST(draw_list_clear_starts_a_new_frame) {
    Fixture f;
    for (int i = 0; i < kCapacity + 1; ++i) f.list.record(f.fill({0, 0}, {1, 1}));
    f.list.clear();
    CHECK(f.list.count() == 0 && f.list.dropped() == 0);
    CHECK(f.build().items == 0);
}

NNH_TEST(a_pass_contains_only_the_draws_of_its_screen) {
    Fixture f;
    f.list.record(f.fill({1, 0}, {1, 1}, 0, Screen::Top));
    f.list.record(f.fill({2, 0}, {1, 1}, 0, Screen::Bottom));
    f.list.record(f.fill({3, 0}, {1, 1}, 0, Screen::Top));
    const auto top = f.build(Screen::Top);
    CHECK(top.items == 2 && g_items[0].x == 1 && g_items[1].x == 3);
    const auto bottom = f.build(Screen::Bottom);
    CHECK(bottom.items == 1 && g_items[0].x == 2);
}

NNH_TEST(a_pass_is_ordered_by_layer_then_by_call) {
    Fixture f;
    f.list.record(f.fill({1, 0}, {1, 1}, 5));
    f.list.record(f.fill({2, 0}, {1, 1}, 0));
    f.list.record(f.fill({3, 0}, {1, 1}, 5));
    f.list.record(f.fill({4, 0}, {1, 1}, 15));
    f.list.record(f.fill({5, 0}, {1, 1}, 0));
    CHECK(f.build().items == 5);
    const float expected[] = {2, 5, 1, 3, 4};
    for (int i = 0; i < 5; ++i) CHECK(g_items[i].x == expected[i]);
    CHECK(g_items[0].layer == 0 && g_items[4].layer == 15);
}

NNH_TEST(layers_outside_0_to_15_are_clamped) {
    Fixture f;
    DrawCommand c = f.fill({1, 0}, {1, 1});
    c.layer = 200;
    f.list.record(c);
    f.list.record(f.fill({2, 0}, {1, 1}, 15));
    CHECK(f.build().items == 2);
    CHECK(g_items[0].layer == 15 && g_items[0].x == 1); // clamped to 15, recorded first
}

NNH_TEST(the_camera_shifts_draws_unless_they_ignore_it) {
    Fixture f;
    f.list.record(f.fill({100, 50}, {10, 10}));
    DrawCommand hud = f.fill({100, 50}, {10, 10});
    hud.ignore_camera = true;
    f.list.record(hud);
    CHECK(f.build(Screen::Top, {30, -20}).items == 2);
    CHECK(g_items[0].x == 70 && g_items[0].y == 70);
    CHECK(g_items[1].x == 100 && g_items[1].y == 50);
}

NNH_TEST(final_positions_are_rounded_to_whole_pixels) {
    Fixture f;
    f.list.record(f.sprite({10.4f, 10.6f}));
    f.list.record(f.sprite({-0.6f, 2.5f}));
    f.list.record(f.sprite({-2.3f, 0}));
    CHECK(f.build(Screen::Top, {0.2f, 0}).items == 3);
    CHECK(g_items[0].x == 10 && g_items[0].y == 11); // 10.2 → 10, 10.6 → 11
    CHECK(g_items[1].x == -1 && g_items[1].y == 3);  // -0.8 → -1, 2.5 → 3
    CHECK(g_items[2].x == -2);                       // -2.5 → -2: halves go up on both sides of 0
}

NNH_TEST(rect_size_follows_its_rounded_corners) {
    Fixture f;
    f.list.record(f.fill({0.6f, 0}, {10, 4.4f}));
    CHECK(f.build().items == 1);
    CHECK(g_items[0].kind == Kind::FillRect);
    CHECK(g_items[0].x == 1 && g_items[0].w == 10); // corners 0.6 → 1 and 10.6 → 11
    CHECK(g_items[0].h == 4);
    f.list.clear();
    f.list.record(f.fill({0.4f, 0}, {10.2f, 1}));
    CHECK(f.build().items == 1);
    CHECK(g_items[0].x == 0 && g_items[0].w == 11); // corners 0.4 → 0 and 10.6 → 11
}

NNH_TEST(a_line_keeps_its_end_point_in_w_and_h) {
    Fixture f;
    DrawCommand line = f.fill({5, 5}, {20, 30});
    line.kind = Kind::Line;
    f.list.record(line);
    CHECK(f.build(Screen::Top, {5, 0}).items == 1);
    CHECK(g_items[0].kind == Kind::Line);
    CHECK(g_items[0].x == 0 && g_items[0].y == 5 && g_items[0].w == 15 && g_items[0].h == 30);
}

NNH_TEST(a_sprite_without_a_region_uses_the_whole_texture) {
    Fixture f;
    f.list.record(f.sprite({0, 0}));
    CHECK(f.build().items == 1);
    const Item& s = g_items[0];
    CHECK(s.kind == Kind::Sprite && s.texture == 42);
    CHECK(s.region.x == 0 && s.region.y == 0 && s.region.w == 64 && s.region.h == 32);
    CHECK(s.w == 64 && s.h == 32);
}

NNH_TEST(a_sprite_region_scale_and_pivot_reach_the_pass) {
    Fixture f;
    DrawCommand c = f.sprite({50, 60});
    c.region = {16, 0, 16, 16};
    c.scale = {2, 3};
    c.pivot = {8, 16};
    c.rotation = 1.5f;
    c.color = nnh::rgba(10, 20, 30, 40);
    f.list.record(c);
    CHECK(f.build().items == 1);
    const Item& s = g_items[0];
    CHECK(s.region.x == 16 && s.region.w == 16 && s.region.h == 16);
    CHECK(s.w == 32 && s.h == 48);
    CHECK(s.pivot_x == 16 && s.pivot_y == 48);
    CHECK(s.x == 50 && s.y == 60 && s.rotation == 1.5f);
    CHECK(s.color.r == 10 && s.color.a == 40);
    CHECK(!s.flip_x && !s.flip_y);
}

NNH_TEST(a_negative_scale_becomes_a_flip) {
    Fixture f;
    DrawCommand c = f.sprite({0, 0});
    c.region = {0, 0, 16, 16};
    c.scale = {-2, 1};
    c.flip_y = true;
    c.pivot = {4, 4};
    f.list.record(c);
    CHECK(f.build().items == 1);
    CHECK(g_items[0].flip_x && g_items[0].flip_y);
    CHECK(g_items[0].w == 32 && g_items[0].pivot_x == 8);
    c.flip_x = true; // flip twice = no flip
    f.list.clear();
    f.list.record(c);
    CHECK(f.build().items == 1);
    CHECK(!g_items[0].flip_x);
}

NNH_TEST(a_sprite_of_a_destroyed_texture_is_skipped_and_counted) {
    Fixture f;
    f.list.record(f.sprite({1, 0}));
    f.list.record(f.fill({2, 0}, {1, 1}));
    TextureData removed{};
    f.textures.remove(f.tex_slot, f.tex_gen, removed);
    const auto result = f.build();
    CHECK(result.items == 1 && result.stale == 1);
    CHECK(g_items[0].kind == Kind::FillRect);
}

namespace {

DrawCommand tiles(int batch, int layer = 0, Screen screen = Screen::Top) {
    DrawCommand c;
    c.screen = screen;
    c.layer = static_cast<std::uint8_t>(layer);
    c.tile_batch = static_cast<std::int16_t>(batch);
    return c;
}

nnh::detail::TileSplit g_splits[4];

} // namespace

NNH_TEST(a_tile_batch_is_a_split_point_among_the_items_by_layer_then_call) {
    Fixture f;
    f.list.record(f.fill({1, 0}, {1, 1}, 2)); // above the map
    f.list.record(tiles(7, 1));               // the map
    f.list.record(f.fill({2, 0}, {1, 1}, 0)); // below
    f.list.record(tiles(3, 2));               // same layer as the first fill, recorded later
    f.list.record(f.fill({3, 0}, {1, 1}, 1)); // same layer as the map, recorded later
    const auto r = f.list.build(Screen::Top, {}, f.textures, g_items, kCapacity, g_splits, 4);
    CHECK(r.items == 3 && r.splits == 2);
    CHECK(g_items[0].x == 2 && g_items[1].x == 3 && g_items[2].x == 1); // tile commands make no item
    CHECK(g_splits[0].at == 1 && g_splits[0].batch == 7); // after the layer-0 fill, before the layer-1 fill
    CHECK(g_splits[1].at == 3 && g_splits[1].batch == 3); // after everything
}

NNH_TEST(tile_splits_belong_to_their_screen) {
    Fixture f;
    f.list.record(tiles(1, 0, Screen::Bottom));
    f.list.record(tiles(2, 0, Screen::Top));
    const auto top = f.list.build(Screen::Top, {}, f.textures, g_items, kCapacity, g_splits, 4);
    CHECK(top.splits == 1 && g_splits[0].batch == 2 && g_splits[0].at == 0);
    const auto bottom = f.list.build(Screen::Bottom, {}, f.textures, g_items, kCapacity, g_splits, 4);
    CHECK(bottom.splits == 1 && g_splits[0].batch == 1);
}

NNH_TEST(tile_commands_without_split_storage_draw_nothing) {
    Fixture f;
    f.list.record(tiles(0));
    f.list.record(f.fill({1, 0}, {1, 1}));
    const auto r = f.build();
    CHECK(r.items == 1 && r.splits == 0 && g_items[0].kind == Kind::FillRect);
    const auto one = f.list.build(Screen::Top, {}, f.textures, g_items, kCapacity, g_splits, 0);
    CHECK(one.splits == 0);
}

NNH_TEST(a_pass_can_be_built_again_with_the_same_result) {
    Fixture f;
    f.list.record(f.fill({3, 0}, {1, 1}, 2));
    f.list.record(f.fill({1, 0}, {1, 1}, 1));
    CHECK(f.build().items == 2);
    const float first = g_items[0].x;
    CHECK(f.build().items == 2); // e.g. once per eye for stereoscopic 3D
    CHECK(g_items[0].x == first && g_items[1].x == 3);
    CHECK(f.list.count() == 2);
}
