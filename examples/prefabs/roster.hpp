#pragma once
// One prefab per enemy data asset (spec 021 FR-10): each assets/enemies/*.enemy file (data_types.hpp) becomes a prefab
// named by the asset's name ("enemies/imp") with a Body of its size and colour, a Walker at its speed and its Stats,
// tagged "enemy" and "roster". A designer adds an enemy by adding a text file. Load time only.

#include <nnh/prefabs.hpp>

namespace room {

/// Registers the roster into `library`, writing up to `capacity` prefabs into `out`; returns how many (0 when the
/// game has no enemy assets, an error logged).
int register_roster(nnh::PrefabLibrary& library, nnh::Prefab* out, int capacity);

} // namespace room
