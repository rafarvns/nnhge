#pragma once
// Aseprite exports written in memory for the sprite converter's tests (spec 016 T7.2-T7.4): frames in the Hash or the
// Array layout, with the tags and slices a test gives, converted from MemoryFiles at /g/assets/art/hero.ase.json with
// a PNG header of the sheet's size beside it.

#include <string>
#include <vector>

#include "convert.hpp"
#include "sprite_model.hpp"

namespace aseprite_fixture {

struct Frame {
    int x = 0, y = 0, w = 16, h = 16;
    int ms = 100;
    int sx = 0, sy = 0;                   ///< spriteSourceSize x, y (trimmed frames)
    int source_w = 16, source_h = 16;     ///< sourceSize
    const char* extra = "";               ///< more members, e.g. `"rotated": true, `
};

struct Export {
    std::vector<Frame> frames;
    std::string tags = "";   ///< the members of meta.frameTags, e.g. `{"name": "run", "from": 0, "to": 3}`
    std::string slices = ""; ///< the members of meta.slices
    bool array = false;      ///< the Array layout
    std::string image = "hero.png";
    int sheet_w = 64, sheet_h = 16; ///< meta.size
    int png_w = 64, png_h = 16;     ///< the PNG's own size
    bool png = true;                ///< write the PNG
};

/// `count` untrimmed 16×16 frames in a row, 100 ms each.
Export row(int count);

/// The JSON text of an export, one frame per line from line 3 (Hash: `"hero N.aseprite": {...}`).
std::string json(const Export& e);

/// Converts `json(e)` (or `text` when given). True on success; `problem` otherwise.
bool convert(const Export& e, nnh::assetc::Converted& out, nnh::assetc::Problem& problem, const std::string& text = "");

/// The model read from an export (frames, meta, tags, slices), without writing. True on success.
bool read(const Export& e, nnh::assetc::SpriteModel& model, nnh::assetc::Problem& problem);

} // namespace aseprite_fixture
