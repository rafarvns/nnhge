#pragma once
// Tiled custom properties → the map model (spec 012 FR-6, plan question 4): bool, int, float and string as they
// are; `color` → string "#AARRGGBB" (as Tiled writes it); `file` → string (the path as written); `object` → int
// (the object's id); `class` refused. Names are 1..255 bytes and unique per owner.

#include <string>

#include "json.hpp"
#include "map_model.hpp"

namespace nnh::assetc {

/// Reads a "properties" array (a missing Ref = none). `where` names the owner in messages ("tile 3 of tileset
/// tiles/world"). False with `error` set ("<where>: ...").
bool read_properties(json::Ref array, const std::string& where, Properties& out, std::string& error);

/// Takes the bool property `name` out of `props`: `value` is its value, false when absent. False (with `error`)
/// when it is there but not a bool.
bool take_bool(Properties& props, const char* name, const std::string& where, bool& value, std::string& error);

} // namespace nnh::assetc
