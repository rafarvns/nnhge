#pragma once
// The one way core reaches the live module (spec 030 D51): a table of functions the module's first registration
// publishes. A game that never calls live:: leaves it null and links none of the module (D39's and D49's ops pattern,
// without a static initialiser). Debug/Development only: every use sits in a discarded branch in Release.

#include <cstdint>

namespace nnh::platform {
struct InputSample;
}

namespace nnh::detail {

/// What the overlay's live pages read from the engine's controls on a tick (dev_controls.hpp fills it).
struct DevInput;

struct LiveOps {
    /// The application point (FR-3): queued remote operations and pending edits applied, callbacks and actions run.
    /// Only for game frames, right after the frame counter advanced.
    void (*frame_start)(std::uint32_t frame);
    /// The frame end (after the game's update and draw, before the overlay): follows answered, dumps written, per-frame
    /// watches swept. `paused`: a paused tick (frame = the last game frame).
    void (*frame_end)(std::uint32_t frame, bool paused);
    /// The Stats page's line (`live 12/128 tun ...`); false when there is nothing to say.
    bool (*stats_line)(char* out, int size);
    /// The panel on page 1 (Tunables) or 2 (Watches): input from the engine's controls this tick.
    void (*panel_input)(int page, const DevInput& input, bool paused);
    /// Draws page 1 or 2 into the debug pass.
    void (*panel_draw)(int page, std::uint32_t frame, bool paused);
    /// Selects a row by name (live::show_panel), opening its groups.
    void (*panel_select)(int page, const char* name);
    /// A remote operation (LiveSet or Live, remote/live_wire.hpp) due now: queued for the application point, or
    /// answered at once (catalogue, follows while paused). `seq` answers refusals.
    void (*remote_op)(std::uint8_t op, std::uint8_t arg, std::uint16_t count, std::uint16_t seq, std::uint32_t hash,
                      std::uint32_t bits, std::uint32_t frame, bool paused);
    /// The PC detached (follows dropped, live records off).
    void (*remote_detached)();
};

/// Null until the first live:: registration (debug/live_hook.cpp).
extern const LiveOps* g_live_ops;

} // namespace nnh::detail
