// Aseprite exports for the converter's tests (see aseprite_fixture.hpp).

#include "aseprite_fixture.hpp"

#include "aseprite_frames.hpp"
#include "aseprite_slices.hpp"
#include "aseprite_tags.hpp"
#include "convert_sprite.hpp"
#include "files.hpp"
#include "json.hpp"
#include "tiled_fixture.hpp"

namespace aseprite_fixture {

namespace {

const char kJson[] = "/g/assets/art/hero.ase.json";
const char kPng[] = "/g/assets/art/hero.png";

std::string n(int v) {
    return std::to_string(v);
}

std::string frame_text(const Frame& f) {
    return std::string("{") + f.extra + "\"frame\": {\"x\": " + n(f.x) + ", \"y\": " + n(f.y) + ", \"w\": " + n(f.w) +
           ", \"h\": " + n(f.h) + "}, \"rotated\": false, \"trimmed\": " +
           (f.w != f.source_w || f.h != f.source_h ? "true" : "false") + ", \"spriteSourceSize\": {\"x\": " + n(f.sx) +
           ", \"y\": " + n(f.sy) + ", \"w\": " + n(f.w) + ", \"h\": " + n(f.h) + "}, \"sourceSize\": {\"w\": " +
           n(f.source_w) + ", \"h\": " + n(f.source_h) + "}, \"duration\": " + n(f.ms) + "}";
}

} // namespace

Export row(int count) {
    Export e;
    for (int i = 0; i < count; ++i) {
        Frame f;
        f.x = 16 * i;
        e.frames.push_back(f);
    }
    e.sheet_w = e.png_w = 16 * count;
    return e;
}

std::string json(const Export& e) {
    std::string text = "{\n";
    text += e.array ? " \"frames\": [\n" : " \"frames\": {\n";
    for (std::size_t i = 0; i < e.frames.size(); ++i) {
        const std::string name = "\"hero " + n(static_cast<int>(i)) + ".aseprite\"";
        text += "  ";
        text += e.array ? frame_text(e.frames[i]).insert(1, "\"filename\": " + name + ", ")
                        : name + ": " + frame_text(e.frames[i]);
        text += i + 1 < e.frames.size() ? ",\n" : "\n";
    }
    text += e.array ? " ],\n" : " },\n";
    text += " \"meta\": {\"app\": \"https://www.aseprite.org/\", \"version\": \"1.3.7-x64\", \"image\": \"" + e.image +
            "\", \"format\": \"RGBA8888\", \"size\": {\"w\": " + n(e.sheet_w) + ", \"h\": " + n(e.sheet_h) +
            "}, \"scale\": \"1\",\n";
    text += "  \"frameTags\": [\n" + e.tags + "\n  ],\n";
    text += "  \"layers\": [{\"name\": \"Layer 1\", \"opacity\": 255, \"blendMode\": \"normal\"}],\n";
    text += "  \"slices\": [\n" + e.slices + "\n  ]\n }\n}\n";
    return text;
}

bool convert(const Export& e, nnh::assetc::Converted& out, nnh::assetc::Problem& problem, const std::string& text) {
    nnh::assetc::MemoryFiles files;
    files.add(kJson, text.empty() ? json(e) : text);
    if (e.png) files.add(kPng, tiled_fixture::png(static_cast<std::uint32_t>(e.png_w), static_cast<std::uint32_t>(e.png_h)));
    return nnh::assetc::convert_sprite(files, kJson, "/g/assets", out, problem);
}

bool read(const Export& e, nnh::assetc::SpriteModel& model, nnh::assetc::Problem& problem) {
    nnh::assetc::json::Document doc;
    if (!doc.parse(json(e))) return false;
    const auto root = doc.root();
    model = {};
    return nnh::assetc::read_aseprite_frames(root, kJson, model, problem) &&
           nnh::assetc::read_aseprite_meta(root, kJson, e.image, e.png_w, e.png_h, model, problem) &&
           nnh::assetc::read_aseprite_tags(root, kJson, model, problem) &&
           nnh::assetc::read_aseprite_slices(root, kJson, model, problem);
}

} // namespace aseprite_fixture
