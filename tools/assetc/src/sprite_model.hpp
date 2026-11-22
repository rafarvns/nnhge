#pragma once
// The host-side model of a sprite library read from an Aseprite export (spec 016 FR-17): frames, clips, markers and
// the default pivot, before they are written as a `.nnhsprite` block (sprite_writer.hpp).

#include <cstdint>
#include <string>
#include <vector>

namespace nnh::assetc {

struct SpriteFrameModel {
    int x = 0, y = 0, w = 0, h = 0;          ///< region in the sheet
    int offset_x = 0, offset_y = 0;          ///< where the trimmed region sits in the untrimmed frame
    int source_w = 0, source_h = 0;          ///< the untrimmed frame
    std::int32_t duration = 0;               ///< frame clock ticks (from milliseconds)
    int line = 0;
};

struct SpriteClipModel {
    std::string name;
    int from = 0, to = 0;      ///< sheet frames, inclusive
    std::uint8_t mode = 0;     ///< spritefmt::Mode
    std::uint8_t direction = 0;
    int count = 0;             ///< Once 1, Repeat plays, PingPong passes (0 = forever), Loop 0
    int line = 0;
};

/// A marker tag `@name` / `@name=value`: on every clip frame showing a sheet frame in from..to.
struct SpriteMarkerModel {
    std::string name;
    int value = 0;
    int from = 0, to = 0;
    int line = 0;
};

struct SpriteModel {
    std::vector<SpriteFrameModel> frames;
    std::vector<SpriteClipModel> clips;
    std::vector<SpriteMarkerModel> markers;
    bool has_pivot = false;
    double pivot_x = 0, pivot_y = 0; ///< in the untrimmed frame's pixels (from the `pivot` slice)
    int pivot_line = 0;
};

/// Aseprite milliseconds → frame clock ticks, rounded half up (100 ms = 24 576 = 6 frames).
constexpr std::int32_t ms_to_ticks(std::int64_t ms) {
    return static_cast<std::int32_t>((ms * 6144 + 12) / 25);
}

} // namespace nnh::assetc
