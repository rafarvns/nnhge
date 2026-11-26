#pragma once
// Converting one font asset (spec 029 FR-1..FR-4, D49): a description `<path>.font` (TrueType/OpenType rasterised by
// ttf_raster) or a BMFont `<path>.fnt` with its pages `<path>_<n>.png` beside it, into `.nnhfont` bytes, the files it
// was made from (the depfile) and the build's report line. Tests: tests/test_cli_font.cpp.

#include <cstdint>
#include <string>
#include <vector>

#include "files.hpp"

namespace nnh::assetc {

struct FontInput {
    std::string path;     ///< the .font or .fnt file
    std::string assets;   ///< the assets folder (licence files are asset paths)
    std::string shown;    ///< the asset's name in messages ("fonts/body_16")
    std::vector<std::pair<std::string, std::string>> strings; ///< (language code, string file) for `strings`
};

struct FontOutput {
    std::vector<std::uint8_t> bytes;
    std::vector<std::string> inputs; ///< every file read
    std::string report;              ///< "font fonts/body_16: 420 glyphs, 1 page 512x512 (A4), 262 KB, licence own"
    std::vector<std::string> notes;  ///< skipped characters
};

/// @return "" or the problems, one per line, "file:line: reason" or "file: reason".
std::string convert_font(FileSource& files, const FontInput& in, FontOutput& out);

} // namespace nnh::assetc
