// Frames of a sprite sheet (spec 016 FR-1, FR-2, FR-11; T1.3): grids, explicit regions, trim offsets, pivots in
// 1/16 pixel, and the library's own draw_options().

#include "animation/sprite_frame_math.hpp"
#include "animation/sprite_library_internal.hpp"
#include "nnh/gfx.hpp"

namespace nnh {

namespace {

using namespace detail::spritefmt;

constexpr int kMaxCoordinate = 32767;

/// Checks one frame; nullptr when it fits the format, else why.
const char* frame_problem(const SpriteFrameDesc& f) {
    const Region& r = f.region;
    if (r.w <= 0 || r.h <= 0) return "its region has a zero or negative size";
    if (r.x < 0 || r.y < 0) return "its region starts at a negative position";
    if (r.x > kMaxCoordinate - r.w || r.y > kMaxCoordinate - r.h) return "its region is beyond 32767 pixels";
    if (f.offset_x < 0 || f.offset_y < 0) return "its trim offset is negative";
    const int sw = f.source_width != 0 ? f.source_width : r.w;
    const int sh = f.source_height != 0 ? f.source_height : r.h;
    if (sw < 0 || sh < 0 || f.offset_x > sw - r.w || f.offset_y > sh - r.h) {
        return "its trimmed region doesn't fit its untrimmed size";
    }
    if (sw > 2 * kMaxPivot || sh > 2 * kMaxPivot) return "its untrimmed size is over 4094 pixels";
    if (!(f.pivot.x < 0 || f.pivot.y < 0) && (f.pivot.x > kMaxPivot || f.pivot.y > kMaxPivot)) {
        return "its pivot is beyond 2047 pixels";
    }
    return nullptr;
}

FrameEntry make_entry(const SpriteFrameDesc& f, std::uint8_t pivot) {
    const int sw = f.source_width != 0 ? f.source_width : f.region.w;
    const int sh = f.source_height != 0 ? f.source_height : f.region.h;
    FrameEntry e{};
    e.x = static_cast<std::int16_t>(f.region.x);
    e.y = static_cast<std::int16_t>(f.region.y);
    e.w = static_cast<std::int16_t>(f.region.w);
    e.h = static_cast<std::int16_t>(f.region.h);
    e.offset_x = static_cast<std::int16_t>(f.offset_x);
    e.offset_y = static_cast<std::int16_t>(f.offset_y);
    if (f.pivot.x < 0 || f.pivot.y < 0) {
        const detail::SpritePivot16 p = detail::sprite_default_pivot(pivot, sw, sh);
        e.pivot_x = p.x;
        e.pivot_y = p.y;
    } else {
        e.pivot_x = static_cast<std::int16_t>(f.pivot.x * 16.0f + 0.5f);
        e.pivot_y = static_cast<std::int16_t>(f.pivot.y * 16.0f + 0.5f);
    }
    return e;
}

/// The grid's columns, or 0 (with `why`) when it can't be placed.
int grid_columns(const SpriteGridDesc& g, const char*& why) {
    if (g.frame_width <= 0 || g.frame_height <= 0) why = "frame size must be positive";
    else if (g.count <= 0) why = "count must be positive";
    else if (g.margin < 0 || g.spacing < 0) why = "margin and spacing can't be negative";
    else if (g.columns < 0 || g.image_width < 0 || g.image_height < 0) why = "columns and image size can't be negative";
    else if (g.columns == 0 && g.image_width == 0) why = "give columns or image_width";
    if (why != nullptr) return 0;
    if (g.columns > 0) {
        const std::int64_t width = 2 * static_cast<std::int64_t>(g.margin) +
                                   static_cast<std::int64_t>(g.columns) * (g.frame_width + g.spacing) - g.spacing;
        if (g.image_width > 0 && width > g.image_width) why = "the columns don't fit in image_width";
        return why == nullptr ? g.columns : 0;
    }
    const int columns = (g.image_width - 2 * g.margin + g.spacing) / (g.frame_width + g.spacing);
    if (columns <= 0) why = "no frame fits in image_width";
    return columns > 0 ? columns : 0;
}

} // namespace

Result<int> SpriteLibrary::add_grid(const SpriteGridDesc& g) {
    if (block_ == nullptr) return Error::make(ErrorCode::InvalidArgument, "add_grid: empty library");
    const detail::SpriteView v = detail::sprite_view(block_);
    const char* why = nullptr;
    const int columns = grid_columns(g, why);
    if (why != nullptr) return Error::make(ErrorCode::InvalidArgument, "add_grid: %s", why);
    const int first = v.count(kFrames);
    if (g.count > v.cap(kFrames) - first) {
        return Error::make(ErrorCode::LimitReached, "add_grid: frame %d: the library holds %d frames",
                           v.cap(kFrames), v.cap(kFrames));
    }
    for (int i = 0; i < g.count; ++i) { // all checked before any is added
        const std::int64_t x = g.margin + static_cast<std::int64_t>(i % columns) * (g.frame_width + g.spacing);
        const std::int64_t y = g.margin + static_cast<std::int64_t>(i / columns) * (g.frame_height + g.spacing);
        const bool out_x = g.image_width > 0 && x + g.frame_width > g.image_width - g.margin;
        const bool out_y = g.image_height > 0 && y + g.frame_height > g.image_height - g.margin;
        if (out_x || out_y) {
            return Error::make(ErrorCode::InvalidArgument, "add_grid: frame %d: outside the %dx%d image", first + i,
                               g.image_width, g.image_height);
        }
        if (x > kMaxCoordinate - g.frame_width || y > kMaxCoordinate - g.frame_height) {
            return Error::make(ErrorCode::InvalidArgument, "add_grid: frame %d: beyond 32767 pixels", first + i);
        }
        if (g.frame_width > 2 * kMaxPivot || g.frame_height > 2 * kMaxPivot) {
            return Error::make(ErrorCode::InvalidArgument, "add_grid: frames over 4094 pixels");
        }
    }
    for (int i = 0; i < g.count; ++i) {
        SpriteFrameDesc f;
        f.region = {g.margin + (i % columns) * (g.frame_width + g.spacing),
                    g.margin + (i / columns) * (g.frame_height + g.spacing), g.frame_width, g.frame_height};
        v.frames()[first + i] = make_entry(f, v.header().pivot);
    }
    v.header().count[kFrames] = static_cast<std::uint16_t>(first + g.count);
    return first;
}

Result<int> SpriteLibrary::add_frame(const SpriteFrameDesc& f) {
    if (block_ == nullptr) return Error::make(ErrorCode::InvalidArgument, "add_frame: empty library");
    const detail::SpriteView v = detail::sprite_view(block_);
    const int index = v.count(kFrames);
    if (index >= v.cap(kFrames)) {
        return Error::make(ErrorCode::LimitReached, "add_frame: frame %d: the library holds %d frames", index,
                           v.cap(kFrames));
    }
    if (const char* why = frame_problem(f)) {
        return Error::make(ErrorCode::InvalidArgument, "add_frame: frame %d: %s", index, why);
    }
    v.frames()[index] = make_entry(f, v.header().pivot);
    v.header().count[kFrames] = static_cast<std::uint16_t>(index + 1);
    return index;
}

SpriteFrame SpriteLibrary::frame(int index) const {
    if (block_ == nullptr || index < 0 || index >= frame_count()) return {};
    return detail::sprite_frame_of(detail::sprite_view(block_).frames()[index]);
}

DrawOptions SpriteLibrary::draw_options(int index, bool flip_x, bool flip_y) const {
    DrawOptions o;
    if (block_ == nullptr || index < 0 || index >= frame_count()) return o;
    const FrameEntry& e = detail::sprite_view(block_).frames()[index];
    o.region = {e.x, e.y, e.w, e.h};
    o.pivot = detail::sprite_draw_pivot(e, flip_x, flip_y);
    o.flip_x = flip_x;
    o.flip_y = flip_y;
    return o;
}

} // namespace nnh
