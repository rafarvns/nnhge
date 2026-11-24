#include "room_scene.hpp"

#include <cstring>
#include <nnh/app.hpp>
#include <nnh/audio.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/tilemap.hpp>

#include "check_script.hpp"
#include "demos.hpp"
#include "hud.hpp"

namespace tr = nnh::transition;

namespace {

constexpr int kSlices = 40;

/// Spins a fixed integer loop: a slice of "loading work".
std::uint32_t busy_work(std::uint32_t seed) {
    std::uint32_t x = seed;
    for (int i = 0; i < 20000; ++i) x = x * 1664525u + 1013904223u;
    return x;
}

void log_start(const char* op, const Settings& s, const tr::Options& o) {
    const tr::Timing t = tr::timing();
    NNH_LOG_INFO(example, "transition: start %s %s %s %s out %d hold %d in %d", op,
                 o.effect.kind == tr::Kind::Custom ? "curtain" : fx_name(s.fx), screens_name(s.screens),
                 s.continue_reveal ? "continue" : "reverse", t.out.count, t.hold.count, t.in.count);
}

} // namespace

void RoomScene::covered(Demos& demos, bool play_music) {
    (void)demos;
    if (play_music) (void)nnh::music::play("music/loop", {.fade_in = tr::seconds(tr::timing().in)});
}

void RoomScene::finished(Demos& demos) {
    NNH_LOG_INFO(example, "transition: done frame %llu seen %d", static_cast<unsigned long long>(nnh::frame_count()),
                 demos.input->seen());
}

void RoomScene::on_enter() {
    frames_ = 0;
    NNH_LOG_INFO(example, "transitions: showcase room %d", id_ + 1);
}

bool RoomScene::change(Change kind, int out_frames) {
    Settings settings = demos_.settings;
    if (kind == Change::LoadingStep) settings.fx = FxChoice::Curtain;
    tr::Options o = make_options(settings, *demos_.curtain);
    o.out = nnh::Frames{out_frames};
    o.in = nnh::Frames{out_frames};
    Demos* demos = &demos_;
    o.on_covered = [demos] { covered(*demos, true); };
    o.on_finished = [demos] { finished(*demos); };
    nnh::Scene* target = demos_.rooms[1 - id_];
    if (kind == Change::LoadingStep) {
        o.hold = nnh::Frames{20};
        o.loading = [this] { return slice(); };
        slices_ = 0;
    } else if (kind == Change::LoadingScene) {
        o.on_covered = [] {}; // the music comes back with the loading scene's second transition
        demos_.loading_target = 1 - id_;
        target = demos_.loading;
    }
    if (!tr::replace(*target, o)) return false;
    demos_.input->reset_seen();
    nnh::music::stop(tr::seconds(tr::timing().out)); // the music fades with the picture (FR-17)
    log_start(kind == Change::LoadingScene ? "replace-loading-scene" : "replace", settings, o);
    return true;
}

tr::LoadStep RoomScene::slice() {
    ++slices_;
    (void)busy_work(static_cast<std::uint32_t>(slices_));
    return {static_cast<float>(slices_) / kSlices, slices_ >= kSlices};
}

void RoomScene::controls() {
    namespace input = nnh::input;
    Settings& s = demos_.settings;
    const nnh::Stick pad = input::circle_pad();
    square_.x = square_.x + pad.x * 3;
    square_.y = square_.y - pad.y * 3;
    square_.x = square_.x < 16 ? 16 : (square_.x > 368 ? 368 : square_.x);
    square_.y = square_.y < 16 ? 16 : (square_.y > 208 ? 208 : square_.y);
    const int count = static_cast<int>(FxChoice::Count);
    if (input::pressed(nnh::Button::Right)) s.fx = static_cast<FxChoice>((static_cast<int>(s.fx) + 1) % count);
    if (input::pressed(nnh::Button::Left)) s.fx = static_cast<FxChoice>((static_cast<int>(s.fx) + count - 1) % count);
    if (input::pressed(nnh::Button::L)) s.continue_reveal = !s.continue_reveal;
    if (input::pressed(nnh::Button::R)) {
        s.screens = static_cast<ScreensChoice>((static_cast<int>(s.screens) + 1) % static_cast<int>(ScreensChoice::Count));
    }
    if (input::pressed(nnh::Button::Right) || input::pressed(nnh::Button::Left) || input::pressed(nnh::Button::L) ||
        input::pressed(nnh::Button::R)) {
        NNH_LOG_INFO(example, "settings: %s %s %s", fx_name(s.fx), screens_name(s.screens),
                     s.continue_reveal ? "continue" : "reverse");
    }
    if (input::touch_started()) {
        const nnh::TouchPoint p = input::touch_position();
        s.centre = tr::IrisCenter{{static_cast<float>(p.x), static_cast<float>(p.y)}, nnh::Screen::Bottom};
        NNH_LOG_INFO(example, "iris centre: bottom %d,%d", p.x, p.y);
    }
    if (input::pressed(nnh::Button::A)) change(Change::Plain);
    if (input::pressed(nnh::Button::B)) change(Change::LoadingStep);
    if (input::pressed(nnh::Button::X)) change(Change::LoadingScene);
    if (input::pressed(nnh::Button::Select)) {
        demos_.check->return_to = this;
        nnh::scenes::replace(demos_.check->driver());
        NNH_LOG_INFO(example, "transitions: check started");
    }
    if (input::pressed(nnh::Button::Y)) nnh::scenes::replace(*demos_.bench);
    if (input::pressed(nnh::Button::Start)) nnh::scenes::pop();
}

void RoomScene::shot() {
    const char* mode = shot_mode();
    if (mode == nullptr || frames_ != 20) return;
    const char* names[] = {"fade", "wipe", "split", "blinds", "iris", "diamond", "curtain"};
    for (int i = 0; i < 7; ++i) {
        if (std::strcmp(mode, names[i]) == 0) demos_.settings.fx = static_cast<FxChoice>(i);
    }
    if (std::strcmp(mode, "tall") == 0) {
        demos_.settings.fx = FxChoice::Wipe;
        demos_.settings.screens = ScreensChoice::Tall;
    }
    change(std::strcmp(mode, "loading") == 0 ? Change::LoadingStep : Change::Plain, 40);
}

void RoomScene::update() {
    ++frames_;
    controls();
    shot();
}

void RoomScene::draw() {
    namespace gfx = nnh::gfx;
    gfx::draw_tilemap(nnh::Screen::Top, demos_.world->rooms[id_]);
    gfx::draw(nnh::Screen::Top, demos_.world->tiles, square_,
              {.region = {(kTileSquare % 4) * 16, (kTileSquare / 4) * 16, 16, 16}, .layer = 2});
    draw_hud(demos_.settings, id_, square_);
    demos_.input->look();
}
