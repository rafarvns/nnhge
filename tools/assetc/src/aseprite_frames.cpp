// Frames and meta of an Aseprite export (see aseprite_frames.hpp).

#include "aseprite_frames.hpp"

#include "json_fields.hpp"

namespace nnh::assetc {

namespace {

constexpr int kMaxFrames = 4096;
constexpr int kMaxCoordinate = 32767;

bool fail(Problem& problem, const std::string& file, int line, const std::string& reason) {
    problem = {file, reason, line};
    return false;
}

/// `key` as {x, y, w, h} (or {w, h} when `size_only`), whole numbers in range.
bool read_box(json::Ref object, const char* key, bool size_only, int (&out)[4], std::string& error) {
    const json::Ref box = object.get(key);
    if (!box.is_object()) {
        error = std::string("\"") + key + "\" is missing or not an object";
        return false;
    }
    Fields f(box, std::string("\"") + key + "\"");
    out[0] = size_only ? 0 : static_cast<int>(f.required_integer("x", 0, kMaxCoordinate));
    out[1] = size_only ? 0 : static_cast<int>(f.required_integer("y", 0, kMaxCoordinate));
    out[2] = static_cast<int>(f.required_integer("w", 1, kMaxCoordinate));
    out[3] = static_cast<int>(f.required_integer("h", 1, kMaxCoordinate));
    error = f.error();
    return f.ok();
}

bool read_frame(json::Ref v, const std::string& name, SpriteFrameModel& out, std::string& error) {
    if (!v.is_object()) {
        error = "frame " + name + " is not an object";
        return false;
    }
    Fields f(v, "frame " + name);
    if (f.boolean("rotated", false)) {
        error = "frame " + name + ": rotated frames aren't supported (export from Aseprite, which never rotates)";
        return false;
    }
    (void)f.boolean("trimmed", false);
    const std::int64_t ms = f.required_integer("duration", 1, 65535);
    if (!f.ok()) {
        error = f.error();
        return false;
    }
    int region[4], source[4], placed[4];
    if (!read_box(v, "frame", false, region, error)) {
        error = "frame " + name + ": " + error;
        return false;
    }
    out = {};
    out.x = region[0];
    out.y = region[1];
    out.w = region[2];
    out.h = region[3];
    out.source_w = out.w;
    out.source_h = out.h;
    out.duration = ms_to_ticks(ms);
    if (v.get("spriteSourceSize") || v.get("sourceSize")) {
        if (!read_box(v, "spriteSourceSize", false, placed, error) || !read_box(v, "sourceSize", true, source, error)) {
            error = "frame " + name + ": " + error;
            return false;
        }
        if (placed[2] != out.w || placed[3] != out.h) {
            error = "frame " + name + ": \"spriteSourceSize\" must have the size of \"frame\"";
            return false;
        }
        if (placed[0] + placed[2] > source[2] || placed[1] + placed[3] > source[3]) {
            error = "frame " + name + ": the trimmed frame doesn't fit in \"sourceSize\"";
            return false;
        }
        out.offset_x = placed[0];
        out.offset_y = placed[1];
        out.source_w = source[2];
        out.source_h = source[3];
    }
    if (out.x + out.w > kMaxCoordinate || out.y + out.h > kMaxCoordinate) {
        error = "frame " + name + " is beyond 32767 pixels";
        return false;
    }
    return true;
}

} // namespace

bool read_aseprite_frames(json::Ref root, const std::string& file, SpriteModel& model, Problem& problem) {
    if (!root.is_object()) return fail(problem, file, root.line(), "not an Aseprite export (the top is not an object)");
    const json::Ref frames = root.get("frames");
    if (!frames.is_object() && !frames.is_array()) {
        return fail(problem, file, root.line(), "\"frames\" is missing (export the sprite sheet with JSON data)");
    }
    if (frames.size() == 0) return fail(problem, file, frames.line(), "the export has no frames");
    if (frames.size() > kMaxFrames) return fail(problem, file, frames.line(), "more than 4096 frames");
    int index = 0;
    for (json::Ref v = frames.first(); v; v = v.next(), ++index) {
        std::string name = std::to_string(index);
        if (frames.is_object()) name += " (\"" + std::string(v.key()) + "\")";
        SpriteFrameModel frame;
        std::string error;
        if (!read_frame(v, name, frame, error)) return fail(problem, file, v.line(), error);
        frame.line = v.line();
        model.frames.push_back(frame);
    }
    return true;
}

bool read_aseprite_meta(json::Ref root, const std::string& file, const std::string& png_name, int png_w, int png_h,
                        const SpriteModel& model, Problem& problem) {
    const json::Ref meta = root.get("meta");
    if (!meta.is_object()) return fail(problem, file, root.line(), "\"meta\" is missing");
    const json::Ref image = meta.get("image");
    if (!image.is_string()) {
        return fail(problem, file, meta.line(), "\"meta\": \"image\" is missing (export the sheet as " + png_name + ")");
    }
    if (image.string() != png_name) {
        return fail(problem, file, image.line(),
                    "\"meta\": \"image\" is \"" + std::string(image.string()) + "\": it must be " + png_name +
                        ", the PNG beside the export");
    }
    int size[4];
    std::string error;
    if (!read_box(meta, "size", true, size, error)) return fail(problem, file, meta.line(), "\"meta\": " + error);
    if (size[2] != png_w || size[3] != png_h) {
        return fail(problem, file, meta.get("size").line(),
                    "\"meta\": \"size\" is " + std::to_string(size[2]) + "x" + std::to_string(size[3]) + " but " +
                        png_name + " is " + std::to_string(png_w) + "x" + std::to_string(png_h));
    }
    for (std::size_t i = 0; i < model.frames.size(); ++i) {
        const SpriteFrameModel& f = model.frames[i];
        if (f.x + f.w > png_w || f.y + f.h > png_h) {
            return fail(problem, file, f.line, "frame " + std::to_string(i) + " is outside the " + png_name + " image");
        }
    }
    return true;
}

} // namespace nnh::assetc
