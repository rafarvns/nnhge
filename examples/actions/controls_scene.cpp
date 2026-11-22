#include "controls_scene.hpp"

#include "action_log.hpp"

#include <cstdio>
#include <nnh/gfx.hpp>

namespace {
constexpr int kActions = static_cast<int>(sizeof kRebindable / sizeof kRebindable[0]);
std::uint8_t g_blob[nnh::kMaxBindingsSize]; // saved bytes, for the checksum

/// Checksum of the set's current bindings (what save() writes).
std::uint32_t current_sum(const nnh::ActionSet& set, std::size_t* size = nullptr) {
    const nnh::Result<std::size_t> r = set.save_to(g_blob, sizeof g_blob, kBindingsVersion);
    if (!r) return 0;
    if (size != nullptr) *size = r.value();
    return bindings_sum(g_blob, r.value());
}
} // namespace

void ControlsScene::on_enter() {
    s_.controls.bind_context(Ctx::Menu, *this);
    cursor_ = 0;
    listening_ = false;
    std::snprintf(last_, sizeof last_, "A rebind  X reset  Y reset all  B save");
}

void ControlsScene::update() {
    nnh::ActionSet& c = s_.controls;
    if (listening_) {
        if (!c.listening()) finish_listen();
        return;
    }
    if (c.repeated(Act::Up)) cursor_ = (cursor_ + kItems - 1) % kItems;
    if (c.repeated(Act::Down)) cursor_ = (cursor_ + 1) % kItems;
    if (c.pressed(Act::Confirm)) confirm();
    if (c.pressed(Act::Reset) && cursor_ < kActions) {
        c.reset(kRebindable[cursor_]);
        std::snprintf(last_, sizeof last_, "actions: reset %s", c.name(kRebindable[cursor_]));
        log_line(last_);
    }
    if (c.pressed(Act::ResetAll)) {
        c.reset_all();
        std::snprintf(last_, sizeof last_, "actions: reset all");
        log_line(last_);
    }
    if (c.pressed(Act::Back)) leave();
}

void ControlsScene::confirm() {
    if (cursor_ < kActions) {
        s_.controls.listen(kRebindable[cursor_], 0, {.timeout_frames = 600});
        listening_ = true;
    } else if (cursor_ == kActions) {
        reload();
    } else {
        nnh::scenes::push(*s_.bench_scene);
    }
}

void ControlsScene::finish_listen() {
    listening_ = false;
    const nnh::RemapResult& r = s_.controls.listen_result();
    format_remap(last_, sizeof last_, r, s_.controls);
    log_line(last_);
    s_.check.remap(s_.controls.name(nnh::ActionId(r.action)), r.source,
                   r.state == nnh::ListenState::Bound ? r.resolution : nnh::Resolution::Rejected,
                   s_.controls.name(nnh::ActionId(r.other_action)));
}

void ControlsScene::leave() {
    std::size_t size = 0;
    const std::uint32_t sum = current_sum(s_.controls, &size);
    const nnh::Result<void> saved = s_.controls.save(kBindingsPath, kBindingsVersion);
    if (saved) {
        std::snprintf(last_, sizeof last_, "actions: saved %u bytes sum %08X", static_cast<unsigned>(size),
                      static_cast<unsigned>(sum));
        log_line(last_);
        s_.check.saved(sum);
    } else {
        NNH_LOG_WARN(example, "actions: save failed: %s", saved.error().message);
    }
    nnh::scenes::pop();
}

void ControlsScene::reload() {
    const nnh::Result<nnh::LoadReport> r = s_.controls.load(kBindingsPath, kBindingsVersion);
    if (!r) {
        NNH_LOG_WARN(example, "actions: reload failed: %s", r.error().message);
        return;
    }
    const std::uint32_t sum = current_sum(s_.controls);
    std::snprintf(last_, sizeof last_, "actions: reloaded %d applied %d skipped sum %08X", r.value().applied,
                  r.value().skipped, static_cast<unsigned>(sum));
    log_line(last_);
    s_.check.reloaded(sum, r.value().applied, r.value().skipped);
}

void ControlsScene::draw() {
    const nnh::Screen top = nnh::Screen::Top;
    nnh::gfx::fill_rect(top, {40, 24, 320, 192}, nnh::rgba(10, 12, 24, 230));
    nnh::gfx::draw_text(top, {200, 32}, "CONTROLS", {.scale = 2, .align = nnh::TextAlign::Center});
    for (int i = 0; i < kItems; ++i) {
        const float y = 64.0f + 18.0f * static_cast<float>(i);
        const nnh::Color color = i == cursor_ ? nnh::colors::yellow : nnh::colors::white;
        if (i == cursor_) nnh::gfx::draw_text(top, {52, y}, ">", {.color = color});
        if (i < kActions) {
            char sources[64];
            s_.controls.describe(kRebindable[i], sources, sizeof sources);
            nnh::gfx::draw_text(top, {64, y}, s_.controls.name(kRebindable[i]), {.color = color});
            nnh::gfx::draw_text(top, {144, y}, sources, {.color = color});
        } else {
            nnh::gfx::draw_text(top, {64, y}, i == kActions ? "Reload bindings" : "Bench", {.color = color});
        }
    }
    const char* hint = listening_ ? "Press a control... (START cancels)" : last_;
    nnh::gfx::draw_text(top, {200, 196}, hint, {.color = nnh::rgb(160, 200, 255), .align = nnh::TextAlign::Center});
}
