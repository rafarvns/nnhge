// Box views over the game's own memory and the query filter (spec 019 FR-3, FR-4, FR-17).

#include "collision/box_view.hpp"
#include "collision/query_check.hpp"
#include "nnh/build.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

#include <cstddef>

namespace collision = nnh::collision;
using collision::BoxView;
using collision::Layers;
using collision::QueryFilter;
using nnh::Rect;

namespace {

bool same(Rect a, Rect b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

enum GameLayer : Layers { kWall = 1u, kFoe = 4u };

struct Padded { // the box after other fields, the layers before it
    char tag;
    GameLayer layer;
    double weight;
    Rect box;
    short hp;
};

struct LayersAfter {
    Rect box;
    Layers layers;
};

constexpr float kNan = __builtin_nanf("");

} // namespace

NNH_TEST(a_plain_array_view_reads_every_box_on_all_layers) {
    const Rect boxes[3] = {{0, 0, 1, 1}, {10, 20, 30, 40}, {-5, -6, 7, 8}};
    const BoxView v = collision::box_view(boxes, 3);
    CHECK(v.count == 3 && v.stride == static_cast<int>(sizeof(Rect)) && v.layers == nullptr);
    for (int i = 0; i < 3; ++i)
        CHECK(same(collision::detail::box_at(v, i), boxes[i]));
    CHECK(collision::detail::layers_at(v, 2) == collision::kAllLayers);
}

NNH_TEST(a_parallel_layer_array_is_read_beside_the_boxes) {
    const Rect boxes[2] = {{0, 0, 1, 1}, {2, 2, 1, 1}};
    const Layers layers[2] = {8u, 3u};
    const BoxView v = collision::box_view(boxes, layers, 2);
    CHECK(collision::detail::layers_at(v, 0) == 8u && collision::detail::layers_at(v, 1) == 3u);
    CHECK(same(collision::detail::box_at(v, 1), boxes[1]));
}

NNH_TEST(a_struct_view_strides_over_padding_and_reads_an_enum_layer_field) {
    Padded items[3] = {};
    for (int i = 0; i < 3; ++i) {
        items[i].tag = 'x';
        items[i].layer = i == 1 ? kFoe : kWall;
        items[i].box = {static_cast<float>(i) * 10, 1, 2, 3};
        items[i].hp = 7;
    }
    const BoxView v = collision::box_view(items, 3, &Padded::box, &Padded::layer);
    CHECK(v.stride == static_cast<int>(sizeof(Padded)) &&
          v.layers_stride == static_cast<int>(sizeof(Padded)));
    CHECK(same(collision::detail::box_at(v, 2), items[2].box));
    CHECK(collision::detail::layers_at(v, 1) == kFoe && collision::detail::layers_at(v, 2) == kWall);
}

NNH_TEST(a_struct_view_with_the_layers_after_the_box) {
    const LayersAfter items[2] = {{{1, 2, 3, 4}, 16u}, {{5, 6, 7, 8}, 32u}};
    const BoxView v = collision::box_view(items, 2, &LayersAfter::box, &LayersAfter::layers);
    CHECK(same(collision::detail::box_at(v, 1), items[1].box));
    CHECK(collision::detail::layers_at(v, 0) == 16u && collision::detail::layers_at(v, 1) == 32u);
}

NNH_TEST(a_struct_view_without_layers_puts_every_box_on_all_layers) {
    const LayersAfter items[1] = {{{1, 2, 3, 4}, 16u}};
    const BoxView v = collision::box_view(items, 1, &LayersAfter::box);
    CHECK(v.layers == nullptr && collision::detail::layers_at(v, 0) == collision::kAllLayers);
}

NNH_TEST(views_of_nothing_are_empty) {
    const BoxView a = collision::box_view(static_cast<const Rect*>(nullptr), 0);
    const BoxView b = collision::box_view(static_cast<const LayersAfter*>(nullptr), 0, &LayersAfter::box);
    CHECK(a.count == 0 && b.count == 0 && b.first == nullptr);
    CHECK(BoxView{}.count == 0);
}

NNH_TEST(the_filter_keeps_boxes_its_mask_detects) {
    const Rect boxes[3] = {{0, 0, 1, 1}, {0, 0, 1, 1}, {0, 0, 1, 1}};
    const Layers layers[3] = {1u, 2u, collision::kNoLayers};
    const BoxView v = collision::box_view(boxes, layers, 3);
    Rect box;
    CHECK(collision::detail::passes(v, {}, 0, box) && collision::detail::passes(v, {}, 1, box));
    CHECK(!collision::detail::passes(v, {}, 2, box)); // on no layers: disabled
    CHECK(!collision::detail::passes(v, {.mask = 2u}, 0, box));
    CHECK(collision::detail::passes(v, {.mask = 2u}, 1, box));
    CHECK(!collision::detail::passes(v, {.mask = collision::kNoLayers}, 1, box));
}

NNH_TEST(the_filter_skips_the_ignored_index_only) {
    const Rect boxes[2] = {{0, 0, 1, 1}, {0, 0, 1, 1}};
    const BoxView v = collision::box_view(boxes, 2);
    Rect box;
    CHECK(!collision::detail::passes(v, {.ignore = 1}, 1, box));
    CHECK(collision::detail::passes(v, {.ignore = 1}, 0, box));
    CHECK(collision::detail::passes(v, {.ignore = 7}, 0, box)); // out of range: nothing ignored
}

NNH_TEST(boxes_without_area_or_with_nan_fields_never_pass) {
    const Rect boxes[5] = {{0, 0, 0, 1}, {0, 0, 1, -1}, {kNan, 0, 1, 1}, {0, 0, 1, kNan}, {0, 0, 2, 3}};
    const BoxView v = collision::box_view(boxes, 5);
    Rect box{};
    for (int i = 0; i < 4; ++i)
        CHECK(!collision::detail::passes(v, {}, i, box));
    CHECK(collision::detail::passes(v, {}, 4, box) && same(box, boxes[4]));
}

NNH_TEST(views_buffers_points_and_grids_are_checked) {
    const Rect boxes[1] = {{0, 0, 1, 1}};
    const Layers layers[1] = {1u};
    using collision::detail::view_ok;
    CHECK(view_ok(collision::box_view(boxes, 1), "t"));
    CHECK(view_ok(BoxView{}, "t"));
    CHECK(view_ok(BoxView{nullptr, 0, 0}, "t")); // nothing to read: any stride
    int out[1];
    CHECK(collision::detail::buffer_ok(out, 1, "t") && collision::detail::buffer_ok(nullptr, 0, "t"));
    CHECK(collision::detail::point_ok({1, 2}, "t") && collision::detail::rect_ok({1, 2, 3, 4}, "t"));
    CHECK(collision::detail::grid_ok(collision::Grid{}, "t"));
    (void)layers;
}

namespace {

const Rect g_boxes[1] = {{0, 0, 1, 1}};
const Layers g_layers[1] = {1u};
bool g_ok = true;
void negative_count() {
    g_ok = collision::detail::view_ok(BoxView{g_boxes, -1}, "t");
}
void null_first() {
    g_ok = collision::detail::view_ok(BoxView{nullptr, 2}, "t");
}
void small_stride() {
    g_ok = collision::detail::view_ok(BoxView{g_boxes, 1, 8}, "t");
}
void small_layer_stride() {
    g_ok = collision::detail::view_ok(BoxView{g_boxes, 1, static_cast<int>(sizeof(Rect)), g_layers, 2}, "t");
}
void null_buffer() {
    g_ok = collision::detail::buffer_ok(nullptr, 3, "t");
}
void negative_capacity() {
    int out[1];
    g_ok = collision::detail::buffer_ok(out, -1, "t");
}
void nan_point() {
    g_ok = collision::detail::point_ok({kNan, 0}, "t");
}
void inf_point() {
    g_ok = collision::detail::point_ok({0, __builtin_inff()}, "t");
}
void nan_rect() {
    g_ok = collision::detail::rect_ok({0, 0, kNan, 1}, "t");
}
void flat_grid() {
    collision::Grid grid;
    grid.cell_h = 0;
    g_ok = collision::detail::grid_ok(grid, "t");
}
void nan_grid() {
    collision::Grid grid;
    grid.cell_w = kNan;
    g_ok = collision::detail::grid_ok(grid, "t");
}

} // namespace

NNH_TEST(each_misuse_asserts_in_debug_and_is_refused_in_release) {
    void (*const cases[])() = {negative_count, null_first,        small_stride, small_layer_stride,
                               null_buffer,    negative_capacity, nan_point,    inf_point,
                               nan_rect,       flat_grid,         nan_grid};
    for (auto fn : cases) {
        g_ok = true;
        CHECK(nnh::platform_fake::run_expecting_crash(fn) == nnh::build::kAssertsEnabled);
        if (!nnh::build::kAssertsEnabled) CHECK(!g_ok);
    }
}
