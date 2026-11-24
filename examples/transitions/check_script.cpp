// The self-check's frame, checksum and log lines (check_script.hpp). The steps are in check_steps.cpp.

#include "check_script.hpp"

#include <cstdio>
#include <cstring>

namespace tr = nnh::transition;

namespace {

/// What `screen` shows in `phase` under `o` (the documented resolution of same(); docs/api/transition.md).
tr::Effect screen_effect(const tr::Options& o, nnh::Screen screen, tr::Phase phase) {
    const bool in = phase == tr::Phase::In;
    const tr::Effect top_out = o.effect;
    const tr::Effect top_in = o.reveal.kind != tr::Kind::Same ? o.reveal : o.effect;
    if (screen == nnh::Screen::Top || o.tall) return in ? top_in : top_out;
    const bool bottom_given = o.bottom.kind != tr::Kind::Same;
    const tr::Effect bottom_out = bottom_given ? o.bottom : o.effect;
    if (!in) return bottom_out;
    if (o.bottom_reveal.kind != tr::Kind::Same) return o.bottom_reveal;
    return bottom_given ? bottom_out : top_in;
}

} // namespace

TransitionsCheck::TransitionsCheck()
    : a_(*this, 0, nnh::rgb(200, 60, 60)), b_(*this, 1, nnh::rgb(60, 120, 220)), c_(*this, 2, nnh::rgb(70, 170, 90)) {
    reset();
}

void TransitionsCheck::reset() {
    sum_ = 2166136261u;
    frame_ = 0;
    top_ = 0;
    loading_calls_ = 0;
    current_ = {};
    for (std::uint32_t& c : checkpoints_) c = 0;
}

void TransitionsCheck::mix(const void* data, std::size_t size) {
    const auto* p = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        sum_ ^= p[i];
        sum_ *= 16777619u;
    }
}

void TransitionsCheck::note(Note what, int id) {
    if (done() || frame_ == 0) return; // the script's frames only (not the exits when it leaves)
    const std::int32_t record[3] = {static_cast<std::int32_t>(what), id, frame_};
    mix(record, sizeof record);
}

void TransitionsCheck::mix_screen(nnh::Screen screen) {
    const float coverage = tr::coverage(screen);
    mix(&coverage, sizeof coverage);
    const tr::Phase phase = tr::phase();
    if (phase == tr::Phase::None) return;
    const tr::Effect effect = screen_effect(current_, screen, phase);
    const tr::Placement placement{.tall = current_.tall,
                                  .tall_gap = current_.tall_gap,
                                  .continued = phase == tr::Phase::In && current_.reveal_style == tr::Reveal::Continue};
    static nnh::Rect rects[tr::kMaxEffectRects];
    const std::int32_t n = tr::effect_rects(screen, effect, coverage, rects, tr::kMaxEffectRects, placement);
    mix(&n, sizeof n);
    for (int i = 0; i < n && i < tr::kMaxEffectRects; ++i) {
        const std::int16_t r[4] = {static_cast<std::int16_t>(rects[i].x), static_cast<std::int16_t>(rects[i].y),
                                   static_cast<std::int16_t>(rects[i].w), static_cast<std::int16_t>(rects[i].h)};
        mix(r, sizeof r);
    }
    const nnh::Color color = tr::effect_color(effect, coverage);
    mix(&color, sizeof color);
}

bool TransitionsCheck::tick() {
    if (done()) return false;
    ++frame_;
    run_steps();
    const std::int32_t header[2] = {frame_, static_cast<std::int32_t>(tr::phase())};
    mix(header, sizeof header);
    const float loading = tr::loading_progress();
    mix(&loading, sizeof loading);
    mix_screen(nnh::Screen::Top);
    mix_screen(nnh::Screen::Bottom);
    if (frame_ % kCheckpointEvery != 0) return false;
    checkpoints_[frame_ / kCheckpointEvery - 1] = sum_;
    if (on_checkpoint != nullptr) on_checkpoint(*this);
    if (done() && return_to != nullptr) {
        nnh::scenes::pop();              // the check scene on top of the driver
        nnh::scenes::replace(*return_to); // then the driver
    }
    return true;
}

void format_transitions_checkpoint(char* out, std::size_t size, int frame, std::uint32_t sum) {
    std::snprintf(out, size, "transitions: %d sum %08X", frame, static_cast<unsigned>(sum));
}

void format_transitions_verdict(char* out, std::size_t size, const TransitionsCheck& check) {
    for (int i = 0; i < TransitionsCheck::kFrames / TransitionsCheck::kCheckpointEvery; ++i) {
        if (check.checkpoints()[i] != kTransitionsCheckpoints[i]) {
            std::snprintf(out, size, "transitions: FAIL frame %d expected %08X got %08X",
                          (i + 1) * TransitionsCheck::kCheckpointEvery,
                          static_cast<unsigned>(kTransitionsCheckpoints[i]),
                          static_cast<unsigned>(check.checkpoints()[i]));
            return;
        }
    }
    std::snprintf(out, size, "transitions: PASS %08X", static_cast<unsigned>(check.checksum()));
}

void CheckScene::on_enter() { check_.note(TransitionsCheck::Note::Enter, id_); }
void CheckScene::on_exit() { check_.note(TransitionsCheck::Note::Exit, id_); }
void CheckScene::update() { check_.note(TransitionsCheck::Note::Update, id_); }

void CheckScene::draw() {
    for (nnh::Screen s : {nnh::Screen::Top, nnh::Screen::Bottom}) {
        nnh::gfx::fill_rect(s, {0, 0, static_cast<float>(nnh::screen_width(s)), 240}, color_);
        nnh::gfx::fill_rect(s, {static_cast<float>(16 + 24 * id_), 16, 16, 16}, nnh::colors::white, {.layer = 15});
    }
}

void CheckDriver::on_enter() {
    check_.reset();
    nnh::scenes::push(check_.scene(0));
}

void CheckDriver::draw() { check_.tick(); }
