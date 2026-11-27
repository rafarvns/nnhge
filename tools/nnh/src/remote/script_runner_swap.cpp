// A script's reload steps (spec 031 FR-22, Q11): `swap-asset` and `restore-asset` close a segment like `expect`. Held
// (the default): the source is swapped, Pause goes out for the clock's frame F, and once the game says it is paused the
// controller builds and sends the batch, its Apply for F and a Resume for F, so the batch lands exactly on F on every
// system; the script goes on at F + 1. `unheld`: the swap and the cycle only (the batch lands when it arrives);
// `expect-reload ... within N` waits for it. `expect-reload` checks the last swap's outcome. A held swap refused by the
// build (no batch) goes on once its notice and Resume have been taken (reload_flushed()).

#include "reload/asset_swap.hpp"
#include "reload/reload_controller.hpp"
#include "reload/reload_names.hpp"
#include "remote/live_wire.hpp"
#include "remote/script_reload.hpp"
#include "remote/script_runner.hpp"

namespace nnh::tool::remote {
namespace {

std::string fixture_path(const std::string& script_dir, const std::string& fixture) {
    return script_dir.empty() ? fixture : script_dir + "/" + fixture;
}

} // namespace

void ScriptRunner::set_reload_script(reload::AssetSwaps* swaps, std::string script_dir) {
    swaps_ = swaps;
    script_dir_ = std::move(script_dir);
}

bool ScriptRunner::reload_step_due() const {
    const Segment& seg = segments_[segment_];
    return is_reload_command(seg.until.command) && next_event_ == seg.events.size() && reload_started_ != segment_ + 1;
}

void ScriptRunner::start_reload_step(const Step& step, std::uint32_t frame) {
    reload_started_ = segment_ + 1;
    reload_step_ = ReloadStepState{0, step, frame, 0};
    if (step.command == Command::ExpectReload) {
        expect_reload_now();
        return;
    }
    if (reload_ == nullptr || swaps_ == nullptr || !reload_->enabled()) {
        finish(Verdict::Error, step.line, "hot reload isn't available: a Release build, or a game built before spec 031");
        return;
    }
    const std::string error = step.command == Command::SwapAsset
                                  ? swaps_->swap(step.text, fixture_path(script_dir_, step.value))
                                  : swaps_->restore(step.text);
    if (!error.empty()) {
        finish(Verdict::Fail, step.line, error);
        return;
    }
    last_reload_ = LastReload{};
    last_reload_.any = true;
    last_reload_.done = false;
    reload_source_ = step.text; // names a refusal; one outside the assets folder is checked against the program
    if (step.unheld) {
        reload_->run_cycle(0, {}, reload_source_);
        take_reload_notes();
        if (reload_->outcome().done) record_outcome();
        complete_reload_step(frame); // the batch lands when it arrives: no frame of the clock
        return;
    }
    wire::Message pause = detail::remote::pause_message(detail::remote::PauseOp::Pause, 0);
    pause.frame = frame; // the game stops before running F
    send(pause, step.line, "");
    reload_step_.phase = 1;
}

void ScriptRunner::reload_paused(std::uint32_t frame, bool paused) {
    if (reload_step_.phase != 1 || !paused || frame < reload_step_.frame) return;
    wire::Message resume = detail::remote::pause_message(detail::remote::PauseOp::Resume, 0);
    resume.frame = reload_step_.frame;
    resume.seq = ++seq_;
    sent_[resume.seq] = Sent{.line = reload_step_.step.line, .name = "", .screens_left = 0};
    std::vector<std::uint8_t> tail(wire::kMessageSize);
    wire::encode_message(resume, tail.data());
    reload_step_.phase = 2;
    reload_->run_cycle(reload_step_.frame, tail, reload_source_);
    take_reload_notes();
    // Refused before a batch (the build): the notice and the Resume are queued; the next steps wait until they're out,
    // or a later-stamped message would overtake the Resume and the game would reject it (frame order).
    if (reload_->outcome().done) reload_step_.phase = 4;
}

void ScriptRunner::reload_flushed() {
    if (reload_step_.phase == 4 && !reload_->sending()) finish_held_reload();
}

void ScriptRunner::finish_held_reload() {
    record_outcome();
    const std::uint32_t at = last_reload_.applied ? last_reload_.frame : reload_step_.frame;
    complete_reload_step(at + 1);
}

void ScriptRunner::record_outcome() {
    const reload::ReloadController::Outcome& o = reload_->outcome();
    last_reload_.done = true;
    last_reload_.applied = o.applied;
    last_reload_.frame = o.frame;
    last_reload_.paths = o.paths;
    last_reload_.why = o.why;
}

void ScriptRunner::reload_outcome() {
    if (reload_ == nullptr || !reload_->outcome().done) return;
    if (reload_step_.phase == 2) {
        finish_held_reload();
    } else if (!last_reload_.done && last_reload_.any) {
        record_outcome();
        if (reload_step_.phase == 3) expect_reload_now();
    }
}

void ScriptRunner::expect_reload_now() {
    const Step& step = reload_step_.step;
    if (!last_reload_.any) {
        finish(Verdict::Fail, step.line, "expect-reload: no swap-asset or restore-asset before it");
        return;
    }
    if (!last_reload_.done) {
        if (step.frames == 0) {
            finish(Verdict::Fail, step.line, "expect-reload: the reload isn't answered yet (an unheld swap needs within N)");
            return;
        }
        if (reload_step_.phase != 3) {
            reload_step_.phase = 3;
            reload_step_.wait_to = reload_step_.frame + step.frames;
        }
        return;
    }
    const bool waited = reload_step_.phase == 3;
    if (last_reload_.applied == step.refused) {
        finish(Verdict::Fail, step.line, last_reload_.applied ? "expect-reload refused: it was applied"
                                                              : "expect-reload: refused (" + last_reload_.why + ")");
        return;
    }
    if (!step.text.empty() && !step.refused) {
        bool found = false;
        for (const std::string& p : last_reload_.paths) found = found || detail::reload::matches(step.text.c_str(), p.c_str());
        if (!found) {
            finish(Verdict::Fail, step.line, "expect-reload: " + step.text + " isn't in the batch");
            return;
        }
    }
    notes_.push_back("line " + std::to_string(step.line) + ": expect-reload " +
                     (last_reload_.applied ? "applied at frame " + std::to_string(last_reload_.frame) : "refused"));
    complete_reload_step(waited && last_reload_.applied ? last_reload_.frame + 1 : reload_step_.frame);
}

void ScriptRunner::reload_tick() {
    if (reload_step_.phase == 3 && last_tick_ > reload_step_.wait_to) {
        finish(Verdict::Fail, reload_step_.step.line,
               "expect-reload: no answer by frame " + std::to_string(reload_step_.wait_to));
    }
}

void ScriptRunner::complete_reload_step(std::uint32_t base) {
    reload_step_.phase = 0;
    if (verdict_ != Verdict::Running) return;
    ++segment_;
    begin_segment(base);
}

void ScriptRunner::take_reload_notes() {
    for (std::string& n : reload_->take_notes()) notes_.push_back(std::move(n));
}

} // namespace nnh::tool::remote
