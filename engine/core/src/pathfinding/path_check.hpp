#pragma once
// Misuse reporting for pathfinding (spec 025 FR-23): an error log on channel `path` and an assert (Debug/Development);
// in Release the caller does nothing harmful. Only the misuse paths call these: the normal path never logs.

#include "nnh/log.hpp"
#include "nnh/pathfinding.hpp"

NNH_LOG_CHANNEL_EXTERN(path);

namespace nnh::detail {

/// Logs and asserts with `what` (a full sentence naming the call).
void path_misuse(const char* what);

/// True when (x, y) is a cell of a width x height map; otherwise reports `who`.
bool cell_ok(int width, int height, int x, int y, const char* who);
/// A buffer: capacity >= 0 and not (null with capacity > 0); otherwise reports `who`.
bool path_buffer_ok(const void* out, int capacity, const char* who);
/// A finite point; otherwise reports `who`.
bool path_point_ok(Vec2 p, const char* who);
/// A weight for set_cell/fill: 1..255 or kPathBlocked; otherwise reports `who`.
bool weight_ok(int weight, const char* who);

/// An inclusive range of map cells.
struct CellSpan {
    int x0 = 0, y0 = 0, x1 = -1, y1 = -1;
    bool empty() const { return x1 < x0 || y1 < y0; }
};
/// `region` clipped to a width x height map. A negative size, or a non-empty region entirely outside the map, is
/// misuse (reported, false); an empty region gives an empty span.
bool clip_region(CellRect region, int width, int height, CellSpan& span, const char* who);

} // namespace nnh::detail
