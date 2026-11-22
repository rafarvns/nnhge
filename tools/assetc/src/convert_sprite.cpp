// One whole sprite conversion (see convert_sprite.hpp).

#include "convert_sprite.hpp"

#include "aseprite_frames.hpp"
#include "aseprite_slices.hpp"
#include "aseprite_tags.hpp"
#include "json.hpp"
#include "paths.hpp"
#include "png_size.hpp"
#include "sprite_writer.hpp"

namespace nnh::assetc {

namespace {

constexpr const char kSuffix[] = ".ase.json";
constexpr std::size_t kSuffixLength = sizeof kSuffix - 1;

bool fail(Problem& problem, const std::string& file, const std::string& reason) {
    problem = {file, reason, 0};
    return false;
}

/// The PNG beside the export: its size. `png_name` is its file name ("hero.png").
bool read_png(FileSource& files, const std::string& json_file, const std::string& png_path, const std::string& png_name,
              int& w, int& h, Problem& problem) {
    std::string header;
    if (!files.read(png_path, kPngHeaderBytes, header)) {
        return fail(problem, json_file, png_name + " not found: export the sheet beside the data (--sheet " + png_name +
                                            ")");
    }
    if (!png_size(header, w, h)) return fail(problem, json_file, png_name + ": not a PNG file");
    return true;
}

} // namespace

bool convert_sprite(FileSource& files, const std::string& json_path, const std::string& assets_root, Converted& out,
                    Problem& problem) {
    out = {};
    const std::string file = normalize_path(json_path);
    const std::string root = normalize_path(assets_root);
    std::string inside;
    if (file.size() <= kSuffixLength || file.compare(file.size() - kSuffixLength, kSuffixLength, kSuffix) != 0) {
        return fail(problem, file, "an Aseprite export must be named <name>.ase.json");
    }
    if (!path_inside(root, file, inside)) return fail(problem, file, "the export is outside the assets folder " + root);
    std::string text;
    if (!files.read(file, json::kMaxDocument + 1, text)) return fail(problem, file, "file not found");
    json::Document doc;
    if (!doc.parse(text)) return fail(problem, file, doc.error());

    const std::string base = file.substr(0, file.size() - kSuffixLength);
    const std::string png_path = base + ".png";
    const std::size_t slash = base.rfind('/');
    const std::string png_name = (slash == std::string::npos ? base : base.substr(slash + 1)) + ".png";
    int png_w = 0, png_h = 0;
    SpriteModel model;
    const json::Ref top = doc.root();
    if (!read_aseprite_frames(top, file, model, problem)) return false;
    if (!read_png(files, file, png_path, png_name, png_w, png_h, problem)) return false;
    if (!read_aseprite_meta(top, file, png_name, png_w, png_h, model, problem)) return false;
    if (!read_aseprite_tags(top, file, model, problem)) return false;
    if (!read_aseprite_slices(top, file, model, problem)) return false;
    if (!write_sprite(model, file, out.bytes, problem)) return false;
    out.dependencies.push_back(file);
    out.dependencies.push_back(png_path);
    return true;
}

} // namespace nnh::assetc
