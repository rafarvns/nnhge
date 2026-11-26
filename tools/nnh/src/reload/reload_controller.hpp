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
#include "reload/source_watcher.hpp"
#include "remote/game_records.hpp"

namespace nnh::tool::reload {

/// What one reconversion gave.
struct BuildOutcome {
    bool ok = false;                ///< the asset target built
    std::vector<std::string> lines; ///< its output, shown as a build shows it
    std::string restart;            ///< non-empty: the dry run's first command (the change needs a rebuild)
    long long ms = 0;               ///< how long it took
};

struct ControllerSetup {
    std::string out_dir;  ///< the converted assets folder (what the game was packed with)
    std::string src_dir;  ///< the source assets folder (watched)
    std::function<BuildOutcome()> build;
    std::function<long long()> now_ms;
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
    /// One cycle now: build, dry run, diff, encode; the Apply for `apply_frame` (0 = the next frame) follows the batch,
    /// then `tail` (a script's Resume). @return false when nothing was sent (a note says why).
    bool run_cycle(std::uint32_t apply_frame, const std::vector<std::uint8_t>& tail = {});

    void on_record(const remote::Record& record);
    /// What may go to the game now (paced). Messages of other kinds may be sent around it: a batch is never split
    /// inside a message.
    std::vector<std::uint8_t> take_bytes();
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
    std::vector<std::uint8_t> pending_;
    std::size_t allowance_ = 0;
    Outcome outcome_;
    std::vector<std::string> notes_;
};

} // namespace nnh::tool::reload
