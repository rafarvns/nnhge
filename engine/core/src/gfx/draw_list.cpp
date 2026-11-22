// The frame's recorded 2D draws (see draw_list.hpp).

#include "gfx/draw_list.hpp"

#include <cmath>

namespace nnh::detail {
namespace {

using Item = platform::DrawItem2D;
using Kind = Item::Kind;

/// Whole pixels, halves rounded up (the same on every system, so PC and 3DS agree).
float snap(float v) { return std::floor(v + 0.5f); }

/// Fills `item` from `c` for a sprite. @return false if its texture is gone.
bool sprite_item(const DrawCommand& c, Vec2 at, float z, const TextureTable& textures, Item& item) {
    const TextureData* texture = textures.get(c.texture_slot, c.texture_generation);
    if (texture == nullptr) return false;
    item.texture = texture->platform;
    item.region = c.region.empty() ? Region{0, 0, texture->width, texture->height} : c.region;
    // A negative scale mirrors the sprite: same as a flip with a positive scale.
    item.flip_x = c.flip_x != (c.scale.x < 0);
    item.flip_y = c.flip_y != (c.scale.y < 0);
    const float sx = std::fabs(c.scale.x);
    const float sy = std::fabs(c.scale.y);
    item.w = static_cast<float>(item.region.w) * sx * z;
    item.h = static_cast<float>(item.region.h) * sy * z;
    item.pivot_x = c.pivot.x * sx * z;
    item.pivot_y = c.pivot.y * sy * z;
    item.rotation = c.rotation;
    item.x = snap(at.x * z);
    item.y = snap(at.y * z);
    return true;
}

/// `at` = position − camera, `z` = zoom (1 when ignoring the camera). Outlines and lines stay one pixel thick.
void shape_item(const DrawCommand& c, Vec2 at, Vec2 camera, float z, Item& item) {
    item.x = snap(at.x * z);
    item.y = snap(at.y * z);
    if (c.kind == Kind::Line) {
        item.w = snap((c.second.x - camera.x) * z); // end point
        item.h = snap((c.second.y - camera.y) * z);
    } else {
        // Round both corners, so touching rectangles stay touching.
        item.w = snap((at.x + c.second.x) * z) - item.x;
        item.h = snap((at.y + c.second.y) * z) - item.y;
    }
}

} // namespace

void DrawList::reset(DrawCommand* storage, std::uint16_t* order, int capacity) {
    storage_ = storage;
    order_ = order;
    capacity_ = capacity;
    clear();
}

void DrawList::clear() {
    count_ = 0;
    dropped_ = 0;
}

bool DrawList::record(const DrawCommand& command) {
    if (count_ >= capacity_) {
        ++dropped_;
        return false;
    }
    DrawCommand& stored = storage_[count_++];
    stored = command;
    if (stored.layer >= kLayerCount) stored.layer = kLayerCount - 1;
    return true;
}

DrawList::BuildResult DrawList::build(Screen screen, Camera2D camera, const TextureTable& textures,
                                      platform::DrawItem2D* out, int out_capacity, TileSplit* splits,
                                      int split_capacity) const {
    // Stable counting sort by layer of this screen's commands (O(n), no allocation).
    int starts[kLayerCount + 1] = {};
    for (int i = 0; i < count_; ++i) {
        if (storage_[i].screen == screen) ++starts[storage_[i].layer + 1];
    }
    for (int l = 0; l < kLayerCount; ++l) starts[l + 1] += starts[l];
    const int total = starts[kLayerCount];
    for (int i = 0; i < count_; ++i) {
        if (storage_[i].screen == screen) order_[starts[storage_[i].layer]++] = static_cast<std::uint16_t>(i);
    }

    BuildResult result;
    for (int n = 0; n < total && result.items < out_capacity; ++n) {
        const DrawCommand& c = storage_[order_[n]];
        if (c.tile_batch >= 0) {
            if (result.splits < split_capacity) splits[result.splits++] = {result.items, c.tile_batch};
            continue;
        }
        const Vec2 cam = c.ignore_camera ? Vec2{} : camera.position;
        const float z = c.ignore_camera ? 1.0f : camera.zoom; // × 1 is exact: zoom 1 = the unzoomed pass (D33)
        const Vec2 at = c.position - cam;
        Item item;
        item.kind = c.kind;
        item.layer = c.layer;
        item.color = c.color;
        if (c.kind == Kind::Sprite) {
            if (!sprite_item(c, at, z, textures, item)) {
                ++result.stale;
                continue;
            }
        } else {
            shape_item(c, at, cam, z, item);
        }
        out[result.items++] = item;
    }
    return result;
}

} // namespace nnh::detail
