// Fuzz target body of `nnh-assetc sprite` (spec 016 T7.4; see fuzz_assetc.hpp).

#include <cstdlib>
#include <string>
#include <string_view>

#include "animation/sprite_validate.hpp"
#include "convert_sprite.hpp"
#include "files.hpp"
#include "fuzz_assetc.hpp"

namespace nnh::assetc::fuzz {

const char* sprite_seed() {
    return R"({"frames": {
 "h 0.aseprite": {"frame": {"x": 0, "y": 0, "w": 16, "h": 16}, "rotated": false, "trimmed": false,
   "spriteSourceSize": {"x": 0, "y": 0, "w": 16, "h": 16}, "sourceSize": {"w": 16, "h": 16}, "duration": 100},
 "h 1.aseprite": {"frame": {"x": 16, "y": 2, "w": 10, "h": 12}, "rotated": false, "trimmed": true,
   "spriteSourceSize": {"x": 3, "y": 4, "w": 10, "h": 12}, "sourceSize": {"w": 16, "h": 16}, "duration": 50},
 "h 2.aseprite": {"frame": {"x": 32, "y": 0, "w": 16, "h": 16}, "duration": 70},
 "h 3.aseprite": {"frame": {"x": 48, "y": 0, "w": 16, "h": 16}, "duration": 1}
},
"meta": {"image": "hero.png", "size": {"w": 64, "h": 16},
 "frameTags": [{"name": "idle", "from": 0, "to": 1, "direction": "forward", "repeat": "2"},
  {"name": "run", "from": 1, "to": 3, "direction": "pingpong_reverse", "repeat": 3},
  {"name": "back", "from": 0, "to": 3, "direction": "reverse", "color": "#ff0000ff", "data": "x"},
  {"name": "@step", "from": 1, "to": 2}, {"name": "@hit=-5", "from": 3, "to": 3}],
 "slices": [{"name": "box", "keys": []}, {"name": "pivot", "keys": [{"frame": 0,
  "bounds": {"x": 3, "y": 2, "w": 9, "h": 14}, "pivot": {"x": 4, "y": 13}}]}]}})";
}

bool sprite_converts(const char* text, std::size_t size) {
    MemoryFiles files;
    files.add("/g/assets/art/hero.ase.json", std::string(text, size));
    // A 64x16 PNG header (the converter reads only the size).
    static const char kPng[] = "\x89PNG\r\n\x1a\n\0\0\0\x0DIHDR\0\0\0\x40\0\0\0\x10\x08\x06\0\0\0";
    files.add("/g/assets/art/hero.png", std::string(kPng, sizeof kPng - 1));
    Converted out;
    Problem problem;
    if (convert_sprite(files, "/g/assets/art/hero.ase.json", "/g/assets", out, problem)) {
        if (!nnh::detail::validate_sprite_library(out.bytes.data(), out.bytes.size()).ok()) std::abort();
        return true;
    }
    if (problem.file.empty() || problem.reason.empty()) std::abort(); // every refusal names a file and a reason
    return false;
}

int sprite_target(const std::uint8_t* data, std::size_t size) {
    (void)sprite_converts(reinterpret_cast<const char*>(data), size);
    return 0;
}

} // namespace nnh::assetc::fuzz
