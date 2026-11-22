// The `pivot` slice of an Aseprite export (see aseprite_slices.hpp).

#include "aseprite_slices.hpp"

#include "json_fields.hpp"

namespace nnh::assetc {

namespace {

bool fail(Problem& problem, const std::string& file, int line, const std::string& reason) {
    problem = {file, reason, line};
    return false;
}

constexpr int kMaxCoordinate = 32767;

} // namespace

bool read_aseprite_slices(json::Ref root, const std::string& file, SpriteModel& model, Problem& problem) {
    const json::Ref slices = root.get("meta").get("slices");
    if (slices && !slices.is_array()) return fail(problem, file, slices.line(), "\"meta\": \"slices\" must be an array");
    for (json::Ref slice = slices.first(); slice; slice = slice.next()) {
        if (slice.get("name").string() != "pivot") continue;
        const json::Ref key = slice.get("keys").first();
        if (!key.is_object()) return fail(problem, file, slice.line(), "slice \"pivot\" has no keys");
        const json::Ref bounds = key.get("bounds");
        Fields b(bounds, "slice \"pivot\": \"bounds\"");
        if (!bounds.is_object()) return fail(problem, file, key.line(), "slice \"pivot\": \"bounds\" is missing");
        const double x = static_cast<double>(b.required_integer("x", -kMaxCoordinate, kMaxCoordinate));
        const double y = static_cast<double>(b.required_integer("y", -kMaxCoordinate, kMaxCoordinate));
        const double w = static_cast<double>(b.required_integer("w", 0, kMaxCoordinate));
        const double h = static_cast<double>(b.required_integer("h", 0, kMaxCoordinate));
        if (!b.ok()) return fail(problem, file, bounds.line(), b.error());
        const json::Ref pivot = key.get("pivot");
        if (pivot) {
            Fields p(pivot, "slice \"pivot\": \"pivot\"");
            const double px = static_cast<double>(p.required_integer("x", -kMaxCoordinate, kMaxCoordinate));
            const double py = static_cast<double>(p.required_integer("y", -kMaxCoordinate, kMaxCoordinate));
            if (!pivot.is_object() || !p.ok()) {
                return fail(problem, file, pivot.line(), pivot.is_object() ? p.error() : "\"pivot\" must be an object");
            }
            // ASSUMPTION (spec 016 plan, risk 1): a key's pivot is relative to its bounds, as `center` is in
            // Aseprite's docs; not confirmed by a real export. If it turns out absolute, drop the `+ x`/`+ y` here.
            model.pivot_x = x + px, model.pivot_y = y + py;
        } else {
            model.pivot_x = x + w / 2; // bottom-centre of the slice
            model.pivot_y = y + h;
        }
        model.has_pivot = true;
        model.pivot_line = key.line();
        return true; // the first `pivot` slice
    }
    return true;
}

} // namespace nnh::assetc
