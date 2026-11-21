#include "gate_run.hpp"

GateRun::GateRun(const GateConfig& config) : config_(config) {
    start(config_.target);
}

int GateRun::trial_length() const {
    return config_.skip_frames +
           (phase_ == GatePhase::Target ? config_.target_frames : config_.search_frames);
}

void GateRun::start(GateCounts counts) {
    current_ = GateTrial{.counts = counts};
    position_ = 0;
}

GateStep GateRun::frame(double ms) {
    if (done()) return GateStep::None;
    if (position_++ >= config_.skip_frames) {
        ++current_.frames;
        current_.total_ms += ms;
        if (ms > current_.worst_ms) current_.worst_ms = ms;
        if (ms > config_.missed_ms) ++current_.missed;
    }
    if (position_ < trial_length()) return GateStep::None;
    last_ = current_;
    ++trials_;
    switch (phase_) {
    case GatePhase::Target:
        target_ = last_;
        layer_limit_ = passed() ? target_.counts.layers : 0;
        phase_ = GatePhase::Layers;
        start({target_.counts.layers + (passed() ? 1 : -1), target_.counts.sprites});
        return GateStep::Trial;
    case GatePhase::Layers:
        return next_layer_trial();
    case GatePhase::Sprites:
        return next_sprite_trial();
    case GatePhase::Done:
        break;
    }
    return GateStep::None;
}

GateStep GateRun::next_layer_trial() {
    const int layers = last_.counts.layers;
    if (last_.ok()) layer_limit_ = layers;
    // Going up: stop at the first failure or at the most layers; going down: at the first pass or the least.
    const bool settled = passed() ? (!last_.ok() || layers >= config_.max_layers)
                                  : (last_.ok() || layers <= config_.min_layers);
    if (!settled) {
        start({layers + (passed() ? 1 : -1), last_.counts.sprites});
        return GateStep::Trial;
    }
    phase_ = GatePhase::Sprites;
    sprite_limit_ = passed() ? target_.counts.sprites : -1;
    const int step = passed() ? config_.sprite_step : -config_.sprite_step;
    start({target_.counts.layers, target_.counts.sprites + step});
    return GateStep::LayerLimit;
}

GateStep GateRun::next_sprite_trial() {
    const int sprites = last_.counts.sprites;
    if (last_.ok()) sprite_limit_ = sprites;
    const bool settled =
        passed() ? (!last_.ok() || sprites >= config_.max_sprites) : (last_.ok() || sprites <= 0);
    if (!settled) {
        const int step = passed() ? config_.sprite_step : -config_.sprite_step;
        start({target_.counts.layers, sprites + step});
        return GateStep::Trial;
    }
    phase_ = GatePhase::Done;
    return GateStep::Finished;
}
