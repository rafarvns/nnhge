// Frame tags of an Aseprite export (see aseprite_tags.hpp).

#include "aseprite_tags.hpp"

#include <cmath>

#include "animation/sprite_format.hpp"
#include "json_fields.hpp"
#include "nnh/detail/name_hash.hpp"

namespace nnh::assetc {

namespace {

using namespace nnh::detail::spritefmt;

bool fail(Problem& problem, const std::string& file, int line, const std::string& reason) {
    problem = {file, reason, line};
    return false;
}

/// `repeat`: absent → 0; "3" or 3 → 3. -1 when it is anything else.
int read_repeat(json::Ref tag) {
    const json::Ref v = tag.get("repeat");
    if (!v) return 0;
    if (v.is_number()) {
        const double n = v.number();
        return std::floor(n) == n && n >= 0 && n <= kMaxRepeat ? static_cast<int>(n) : -1;
    }
    const std::string_view text = v.string();
    if (!v.is_string() || text.empty() || text.size() > 5) return -1;
    int n = 0;
    for (char c : text) {
        if (c < '0' || c > '9') return -1;
        n = n * 10 + (c - '0');
    }
    return n <= kMaxRepeat ? n : -1;
}

/// `@name` or `@name=value`.
bool read_marker(const std::string& tag, SpriteMarkerModel& out, std::string& error) {
    const std::size_t equals = tag.find('=');
    out.name = tag.substr(1, equals == std::string::npos ? std::string::npos : equals - 1);
    if (out.name.empty() || out.name.size() > static_cast<std::size_t>(kMaxNameLength)) {
        error = "marker tag \"" + tag + "\": the name after '@' must be 1 to 31 bytes";
        return false;
    }
    out.value = 0;
    if (equals == std::string::npos) return true;
    const std::string value = tag.substr(equals + 1);
    std::size_t i = value.size() > 0 && value[0] == '-' ? 1 : 0;
    long long n = 0;
    bool ok = i < value.size() && value.size() <= 6;
    for (; ok && i < value.size(); ++i) {
        ok = value[i] >= '0' && value[i] <= '9';
        n = n * 10 + (value[i] - '0');
    }
    if (value[0] == '-') n = -n;
    if (!ok || n < -32768 || n > 32767) {
        error = "marker tag \"" + tag + "\": the value after '=' must be a whole number from -32768 to 32767";
        return false;
    }
    out.value = static_cast<int>(n);
    return true;
}

bool read_direction(const std::string& direction, int repeat, SpriteClipModel& clip) {
    if (direction == "forward" || direction == "reverse") {
        clip.direction = direction == "reverse" ? kReverse : kForward;
        clip.mode = repeat == 0 ? kLoop : (repeat == 1 ? kOnce : kRepeat);
        clip.count = repeat;
        return true;
    }
    if (direction == "pingpong" || direction == "pingpong_reverse") {
        clip.direction = direction == "pingpong_reverse" ? kReverse : kForward;
        clip.mode = kPingPong;
        clip.count = repeat; // one-way passes, as Aseprite counts them
        return true;
    }
    return false;
}

bool read_tag(json::Ref tag, int frames, const std::string& file, SpriteModel& model, Problem& problem) {
    if (!tag.is_object()) return fail(problem, file, tag.line(), "a frame tag is not an object");
    Fields f(tag, "frame tag");
    const std::string name = f.required_string("name");
    const int from = static_cast<int>(f.required_integer("from", 0, frames - 1));
    const int to = static_cast<int>(f.required_integer("to", 0, frames - 1));
    const std::string direction = f.string("direction", "forward");
    if (!f.ok()) {
        const std::string what = name.empty() ? f.error() : "frame tag \"" + name + "\"" + f.error().substr(9);
        return fail(problem, file, tag.line(), what + " (the export has " + std::to_string(frames) + " frames)");
    }
    const std::string where = "frame tag \"" + name + "\": ";
    if (from > to) return fail(problem, file, tag.line(), where + "\"from\" is after \"to\"");
    if (!name.empty() && name[0] == '@') {
        SpriteMarkerModel marker;
        std::string error;
        if (!read_marker(name, marker, error)) return fail(problem, file, tag.line(), error);
        marker.from = from;
        marker.to = to;
        marker.line = tag.line();
        model.markers.push_back(marker);
        return true;
    }
    if (name.empty() || name.size() > static_cast<std::size_t>(kMaxNameLength)) {
        return fail(problem, file, tag.line(), where + "the name must be 1 to 31 bytes");
    }
    const int repeat = read_repeat(tag);
    if (repeat < 0) return fail(problem, file, tag.line(), where + "\"repeat\" must be a whole number from 0 to 65535");
    if (to - from + 1 > kMaxClipLength) return fail(problem, file, tag.line(), where + "more than 256 frames");
    SpriteClipModel clip;
    clip.name = name;
    clip.from = from;
    clip.to = to;
    clip.line = tag.line();
    if (!read_direction(direction, repeat, clip)) {
        return fail(problem, file, tag.line(),
                    where + "unknown direction \"" + direction + "\" (forward, reverse, pingpong, pingpong_reverse)");
    }
    for (const SpriteClipModel& other : model.clips) {
        if (other.name == name || nnh::detail::fnv1a32(other.name.c_str()) == nnh::detail::fnv1a32(name.c_str())) {
            return fail(problem, file, tag.line(), where + "two clips have this name (or the same name hash)");
        }
    }
    if (model.clips.size() >= static_cast<std::size_t>(kMaxClips)) {
        return fail(problem, file, tag.line(), "more than 256 clip tags");
    }
    model.clips.push_back(clip);
    return true;
}

} // namespace

bool read_aseprite_tags(json::Ref root, const std::string& file, SpriteModel& model, Problem& problem) {
    const int frames = static_cast<int>(model.frames.size());
    const json::Ref tags = root.get("meta").get("frameTags");
    if (tags && !tags.is_array()) return fail(problem, file, tags.line(), "\"meta\": \"frameTags\" must be an array");
    for (json::Ref tag = tags.first(); tag; tag = tag.next()) {
        if (!read_tag(tag, frames, file, model, problem)) return false;
    }
    if (model.clips.empty()) {
        if (frames > kMaxClipLength) {
            return fail(problem, file, root.line(), "more than 256 frames and no frame tags: tag the clips in Aseprite");
        }
        SpriteClipModel all;
        all.name = "default";
        all.to = frames - 1;
        all.line = root.line();
        model.clips.push_back(all);
    }
    return true;
}

} // namespace nnh::assetc
