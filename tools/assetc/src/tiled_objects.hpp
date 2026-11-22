#pragma once
// A Tiled object layer → the map model (spec 012 FR-5, FR-6, plan question 3): rectangles and points with their id,
// name, type (`type`, or `class` as Tiled 1.9 writes it) and properties. Refused: ellipses, polygons, polylines,
// text, tile objects, templates and rotated objects.

#include <string>
#include <vector>

#include "convert_context.hpp"
#include "json.hpp"
#include "map_model.hpp"

namespace nnh::assetc {

/// Reads one layer of type "objectgroup"; its objects are appended to `objects` with object layer `index`.
bool read_object_layer(json::Ref layer, int index, const std::string& file, Context& ctx, ObjectLayer& out,
                       std::vector<Object>& objects);

} // namespace nnh::assetc
