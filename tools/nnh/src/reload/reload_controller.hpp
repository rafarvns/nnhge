#pragma once
// The reload cycle on the PC (spec 031 FR-1–FR-4, FR-14, FR-15, plan "`nnh` side"): idle → (a save, after the quiet
// period, or a typed `reload`) build the asset target and dry-run the game → diff the outputs against the mirror → one
// batch (or a notice: conversion failed, needs a restart) → send it paced by the game's ticks → wait for ReloadApplied or
// ReloadRefused. Saves during a cycle are gathered into the next one. The build is injected (one call does the asset
// build and the dry run), so the cycle is tested with a fake builder and game. Tests:
// tools/nnh/tests/test_reload_controller.cpp.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "reload/output_snapshot.hpp"
#include "reload/paced_units.hpp"
#include "reload/source_watcher.hpp"
#include "remote/game_records.hpp"

namespace nnh::tool::reload {

/// What one reconversion gave.
struct BuildOutcome {
    bool ok = false;                ///< the asset target built
    std::vector<std::string> lines; ///< its output, shown as a build shows it
    std::string restart;            ///< non-empty: the changed file is an input of the program (rebuild)
    long long ms = 0;               ///< how long it took
};

struct ControllerSetup {
    std::string out_dir;  ///< the converted assets folder (what the game was packed with)
    std::string src_dir;  ///< the source assets folder (watched)
    /// Builds the asset target; with an `outside_source` (an absolute path: a changed file outside the assets folder, a
    /// schema source for instance), then asks Ninja whether it is an input of the program (→ BuildOutcome::restart).
    std::function<BuildOutcome(const std::string& outside_source)> build;
    std::function<long long()> now_ms;
    std::string game_dir; ///< the game's folder (script SOURCE paths are relative to it)
};

/// Bytes sent at the start of a batch before the game's first tick, then per tick (it reads 16 KB a frame, ticks come
/// every 4 frames).
inline constexpr std::size_t kInitialAllowance = 64 * 1024;
inline constexpr std::size_t kAllowancePerTick = 48 * 1024;

class ReloadController {
public:
    explicit ReloadController(ControllerSetup setup);

    /// The session starts: the mirror of the outputs and the sources' listing.
    void start();
    /// Watching the sources (interactive `nnh run`, unless --no-reload; scripts swap explicitly).
    void set_watching(bool on) { watching_ = on; }
    bool watching() const { return watching_ && enabled_; }
    /// False once the game refused a reload message (built before spec 031).
    bool enabled() const { return enabled_; }

    /// The session loop: polls the sources every kPollMs while watching and idle, and starts a cycle after a burst.
    void poll();
    /// Typed `reload` (now, skipping the quiet period) or `reload PATH` (that output sent again).
    void request(const std::string& output_path = "");
    /// One cycle now: build, diff, encode; the Apply for `apply_frame` (0 = the next frame) follows the batch,
    /// then `tail` (a script's Resume). `script_source`: a script's swapped file, relative to the game's folder: it
    /// names a refusal's notice, and one outside the assets folder is checked against the program's inputs
    /// (BuildOutcome::restart). @return false when nothing was sent (a note says why).
    bool run_cycle(std::uint32_t apply_frame, const std::vector<std::uint8_t>& tail = {},
                   const std::string& script_source = "");
    /// True when `game_path` (relative to the game's folder) is inside the source assets folder.
    bool inside_assets(const std::string& game_path) const;

    void on_record(const remote::Record& record);
    /// Builds the asset target once more, sending nothing (after a script put its swapped sources back).
    void rebuild() { setup_.build(""); }
    /// What may go to the game now (paced): whole units only (a message with its payload), so that messages of other
    /// kinds sent around it land on a unit boundary.
    std::vector<std::uint8_t> take_bytes();
    /// Bytes are still waiting for take_bytes().
    bool sending() const { return !pending_.empty(); }
    std::vector<std::string> take_notes();

    enum class State : std::uint8_t { Idle, Sending, Waiting };
    State state() const { return state_; }

    /// The last cycle's result (scripts read it).
    struct Outcome {
        bool done = false;     ///< answered (or nothing to send)
        bool applied = false;
        bool sent = false;     ///< a batch went out
        std::uint32_t frame = 0;
        std::vector<std::string> paths; ///< the batch's asset paths
        std::string why;       ///< a refusal's reason
    };
    const Outcome& outcome() const { return outcome_; }

private:
    void finish_cycle();
    void note(const std::string& text) { notes_.push_back(text); }

    ControllerSetup setup_;
    OutputSnapshot mirror_;
    SourceWatcher watcher_;
    bool watching_ = false;
    bool enabled_ = true;
    bool requested_ = false;
    State state_ = State::Idle;
    long long last_poll_ = 0;
    long long saved_at_ = 0;   ///< the save (or request) that started the cycle
    std::uint16_t batch_ = 0;
    std::uint16_t seq_ = 0xC000;
    std::vector<OutputChange> changes_;
    void set_pending(const std::vector<std::uint8_t>& batch, const std::vector<std::uint8_t>& tail);
    PacedUnits pending_; ///< whole units (review B1)
    std::size_t allowance_ = 0;
    Outcome outcome_;
    std::vector<std::string> notes_;
};

} // namespace nnh::tool::reload
