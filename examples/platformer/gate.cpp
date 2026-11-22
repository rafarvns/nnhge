#include "gate.hpp"

#include <cstdio>
#include <nnh/debug.hpp>
#include <nnh/gfx.hpp>
#include <nnh/log.hpp>

NNH_LOG_CHANNEL_EXTERN(platformer);

namespace {

constexpr nnh::Screen kTop = nnh::Screen::Top;
constexpr nnh::Screen kBottom = nnh::Screen::Bottom;
constexpr GateConfig kConfig{};
const nnh::Color kPass = nnh::rgb(80, 220, 100);
const nnh::Color kFail = nnh::rgb(240, 70, 60);
const nnh::Color kBusy = nnh::rgb(250, 220, 120);
const nnh::Color kText = nnh::rgb(200, 210, 230);

/// "6 layers" / "none (even 1 layer missed)", for the limit lines.
void layer_limit_text(char* out, std::size_t size, int limit) {
    if (limit <= 0)
        std::snprintf(out, size, "none (even %d layer missed)", kConfig.min_layers);
    else
        std::snprintf(out, size, "%d layers%s", limit,
                      limit >= kConfig.max_layers ? " (the most it has)" : "");
}

void sprite_limit_text(char* out, std::size_t size, int limit) {
    if (limit < 0)
        std::snprintf(out, size, "none (even 0 sprites missed)");
    else
        std::snprintf(out, size, "%d sprites%s", limit,
                      limit >= kConfig.max_sprites ? " (the most it has)" : "");
}

} // namespace

void Gate::start(float zoom) {
    run_ = GateRun(kConfig);
    zoom_ = zoom;
    running_ = true;
    finished_ = false;
    NNH_LOG_INFO(platformer, "bench: gate started: %d layers + %d sprites for %d frames, then the limits%s",
                 kConfig.target.layers, kConfig.target.sprites, kConfig.target_frames,
                 measure_only() ? " (zoomed: measure only)" : "");
}

void Gate::stop() {
    if (!running_) return;
    running_ = false;
    NNH_LOG_INFO(platformer, "bench: gate stopped after %d trials (left the benchmark): no verdict",
                 run_.trials());
}

GateCounts Gate::frame(double ms) {
    if (!running_) return run_.counts();
    switch (run_.frame(ms)) {
    case GateStep::None:
        break;
    case GateStep::Trial:
        log_trial();
        break;
    case GateStep::LayerLimit:
        log_trial();
        log_layer_limit();
        break;
    case GateStep::Finished:
        log_trial();
        log_finish();
        running_ = false;
        finished_ = true;
        return run_.target().counts; // back to the manual bench at the target
    }
    return run_.counts();
}

void Gate::log_trial() const {
    const GateTrial& t = run_.last();
    NNH_LOG_INFO(platformer,
                 "bench: trial %d layers + %d sprites: %s, avg %.1f ms, worst %.1f ms, missed %d/%d",
                 t.counts.layers, t.counts.sprites, t.ok() ? "ok" : "MISSED", t.average_ms(), t.worst_ms,
                 t.missed, t.frames);
}

void Gate::log_layer_limit() const {
    char text[48];
    layer_limit_text(text, sizeof text, run_.layer_limit());
    NNH_LOG_INFO(platformer, "bench: limit %s with %d sprites", text, kConfig.target.sprites);
}

void Gate::log_finish() const {
    if (measure_only()) {
        char line[128];
        format_zoom_limits(line, sizeof line, zoom_, run_.layer_limit(), run_.sprite_limit(), kConfig.target);
        NNH_LOG_INFO(platformer, "%s", line);
        format_zoom_done(line, sizeof line, zoom_);
        NNH_LOG_INFO(platformer, "%s", line);
        return;
    }
    char text[48];
    sprite_limit_text(text, sizeof text, run_.sprite_limit());
    NNH_LOG_INFO(platformer, "bench: limit %s with %d layers", text, kConfig.target.layers);
    const GateTrial& t = run_.target();
    if (run_.passed()) {
        NNH_LOG_INFO(platformer,
                     "bench: PASS %d layers + %d sprites, avg %.1f ms, worst %.1f ms, missed 0 of %d frames",
                     t.counts.layers, t.counts.sprites, t.average_ms(), t.worst_ms, t.frames);
    } else {
        NNH_LOG_WARN(platformer,
                     "bench: FAIL %d layers + %d sprites, avg %.1f ms, worst %.1f ms, missed %d of %d frames",
                     t.counts.layers, t.counts.sprites, t.average_ms(), t.worst_ms, t.missed, t.frames);
    }
}

void Gate::draw_panel() const {
    nnh::gfx::draw_text(kBottom, {12, 10}, "TILE BENCHMARK: AUTOMATIC GATE", {.color = kText});
    if (running_)
        draw_progress();
    else if (finished_)
        draw_verdict();
}

void Gate::draw_progress() const {
    const int line = nnh::debug::line_height();
    const GateTrial& t = run_.target();
    nnh::gfx::draw_text(kBottom, {12, 26}, "GATE RUNNING", {.color = kBusy, .scale = 2, .shadow = true});
    const GateCounts c = run_.counts();
    nnh::debug::printf(kBottom, 12, 56, "trial %d: %d layers + %d sprites", run_.trials() + 1, c.layers,
                       c.sprites);
    nnh::debug::printf(kBottom, 12, 56 + line, "frame %d of %d", run_.trial_frame(), run_.trial_length());
    if (run_.trials() > 0) {
        const GateTrial& l = run_.last();
        nnh::debug::printf(kBottom, 12, 56 + 3 * line, "last: %d + %d %s, worst %.1f ms", l.counts.layers,
                           l.counts.sprites, l.ok() ? "ok" : "MISSED", l.worst_ms);
        nnh::debug::printf(kBottom, 12, 56 + 4 * line, "target %d + %d: %s", t.counts.layers,
                           t.counts.sprites, t.ok() ? "ok (PASS so far)" : "MISSED (FAIL)");
    }
    nnh::debug::print(kBottom, 12, 56 + 6 * line, "Wait: no input, no screenshots.");
    nnh::debug::print(kBottom, 12, 56 + 7 * line, "SELECT stops the gate.");
}

void Gate::draw_verdict() const {
    const int line = nnh::debug::line_height();
    const GateTrial& t = run_.target();
    char text[48];
    const bool pass = run_.passed();
    if (measure_only()) {
        std::snprintf(text, sizeof text, "ZOOM %.2f", static_cast<double>(zoom_));
        nnh::gfx::draw_text(kBottom, {12, 26}, text, {.color = kBusy, .scale = 3, .shadow = true});
    } else {
        nnh::gfx::draw_text(kBottom, {12, 26}, pass ? "GATE: PASS" : "GATE: FAIL",
                            {.color = pass ? kPass : kFail, .scale = 3, .shadow = true});
    }
    const int y = 60;
    nnh::debug::printf(kBottom, 12, y, "%d layers + %d sprites at 60 Hz", t.counts.layers, t.counts.sprites);
    nnh::debug::printf(kBottom, 12, y + line, "avg %.1f ms  worst %.1f ms", t.average_ms(), t.worst_ms);
    nnh::debug::printf(kBottom, 12, y + 2 * line, "missed %d of %d frames", t.missed, t.frames);
    layer_limit_text(text, sizeof text, run_.layer_limit());
    nnh::debug::printf(kBottom, 12, y + 4 * line, "limit %s", text);
    nnh::debug::printf(kBottom, 12, y + 5 * line, "  with %d sprites", t.counts.sprites);
    sprite_limit_text(text, sizeof text, run_.sprite_limit());
    nnh::debug::printf(kBottom, 12, y + 6 * line, "limit %s", text);
    nnh::debug::printf(kBottom, 12, y + 7 * line, "  with %d layers", t.counts.layers);
    nnh::debug::printf(kBottom, 12, y + 8 * line, "%d trials; numbers also in the log", run_.trials());
    nnh::debug::print(kBottom, 12, y + 10 * line, "A: run again   L/R/Y/X: manual");
    nnh::debug::print(kBottom, 12, y + 11 * line, "SELECT: next demo   START: exit");
}

void Gate::draw_banner() const {
    if (!finished_) return;
    namespace gfx = nnh::gfx;
    const bool pass = run_.passed();
    const char* verdict = measure_only() ? "MEASURED" : (pass ? "GATE: PASS" : "GATE: FAIL");
    const nnh::Vec2 size = gfx::measure_text(verdict, 4);
    const float x = (400 - size.x) / 2;
    const float y = (240 - size.y) / 2;
    gfx::fill_rect(kTop, {x - 12, y - 10, size.x + 24, size.y + 20}, nnh::rgba(0, 0, 0, 200),
                   {.layer = 3, .ignore_camera = true});
    gfx::draw_text(kTop, {x, y}, verdict,
                   {.color = measure_only() ? kBusy : (pass ? kPass : kFail), .scale = 4, .layer = 3,
                    .ignore_camera = true});
}
