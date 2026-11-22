#include "camera_scene.hpp"

#include "hud.hpp"

#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/tilemap.hpp>

namespace {

using nnh::Screen;
namespace gfx = nnh::gfx;

constexpr float kZooms[] = {1, 1.5f, 2, 3, 0.5f};
constexpr int kZoomCount = 5;
constexpr nnh::Color kPlayer = nnh::rgb(250, 250, 250);
constexpr nnh::Color kSecond = nnh::rgb(255, 150, 40);
constexpr nnh::Color kMarker = nnh::rgb(255, 60, 200);

nnh::Vec2 direction() {
    const nnh::Stick pad = nnh::input::circle_pad();
    nnh::Vec2 d{pad.x, -pad.y};
    if (nnh::input::held(nnh::Button::Left)) d.x = -1;
    if (nnh::input::held(nnh::Button::Right)) d.x = 1;
    if (nnh::input::held(nnh::Button::Up)) d.y = -1;
    if (nnh::input::held(nnh::Button::Down)) d.y = 1;
    return d;
}

} // namespace

CameraScene::CameraScene(const Demos& demos, world::World& world) : demos_(demos), world_(world) {
    player_.position = world::kStart;
}

float CameraScene::zoom() const {
    return kZooms[zoom_index_];
}

void CameraScene::preset() {
    if (demo_is("zoom2")) zoom_index_ = 2;
    if (demo_is("zoom05")) zoom_index_ = 4;
    if (demo_is("tall")) mode_ = Mode::Tall;
    if (demo_is("two")) mode_ = Mode::Two;
    // In the doorway between the two left rooms: the combined view centres it on the seam between the
    // screens.
    if (demo_is("tall")) player_.position = {314, 378};
}

void CameraScene::configure() {
    const bool tall = mode_ == Mode::Tall;
    const nnh::Vec2 smoothing = smoothing_ ? nnh::Vec2{0.2f, 0.2f} : nnh::Vec2{1, 1};
    nnh::CameraSettings s = rig_.settings();
    s.layout = tall ? nnh::CameraLayout::TallWorld : nnh::CameraLayout::Single;
    s.screen = Screen::Top;
    s.dead_zone = nnh::CameraSettings::centered_zone(64, 48);
    s.smoothing = smoothing;
    s.zoom = zoom();
    s.gap = gap_;
    s.has_limits = true;
    s.limits = world_.map.bounds();
    rig_.set(s); // a new zoom keeps the view's centre
    if (mode_ == Mode::Two) {
        nnh::CameraSettings b = s;
        b.screen = Screen::Bottom;
        b.zoom = 0.5f; // the map below shows more
        b.dead_zone = {};
        second_.set(b);
    }
    if (mode_ == Mode::Single) gfx::set_camera(Screen::Bottom, {}); // the HUD screen
}

void CameraScene::on_enter() {
    frame_ = 0;
    configure();
    rig_.snap(player_.box());
    second_.snap(world::auto_target(0));
    NNH_LOG_INFO(example, "camera: playground, zoom %.2f", static_cast<double>(zoom()));
}

void CameraScene::controls() {
    namespace in = nnh::input;
    bool changed = false;
    if (in::pressed(nnh::Button::X)) {
        zoom_index_ = (zoom_index_ + 1) % kZoomCount;
        changed = true;
        NNH_LOG_INFO(example, "camera: zoom %.2f", static_cast<double>(zoom()));
    }
    if (in::pressed(nnh::Button::Y)) smoothing_ = !smoothing_, changed = true;
    if (in::pressed(nnh::Button::L)) gap_ = gap_ == 0 ? 24.0f : 0.0f, changed = true;
    if (in::pressed(nnh::Button::R)) {
        mode_ = mode_ == Mode::Single ? Mode::Tall : (mode_ == Mode::Tall ? Mode::Two : Mode::Single);
        changed = true;
        static const char* const kNames[] = {"single view", "tall world", "two views"};
        NNH_LOG_INFO(example, "camera: mode %s", kNames[static_cast<int>(mode_)]);
    }
    if (in::pressed(nnh::Button::A)) {
        rig_.add_trauma(0.4f);
        second_.add_trauma(0.4f);
    }
    if (changed) configure();
}

void CameraScene::touch() {
    if (mode_ == Mode::Single || !nnh::input::touch_started()) return;
    const nnh::TouchPoint p = nnh::input::touch_position();
    const nnh::CameraRig& below = mode_ == Mode::Tall ? rig_ : second_;
    marker_ = below.to_world(Screen::Bottom, {static_cast<float>(p.x), static_cast<float>(p.y)});
    has_marker_ = true;
    NNH_LOG_INFO(example, "camera: marker at %.1f %.1f", static_cast<double>(marker_.x),
                 static_cast<double>(marker_.y));
}

void CameraScene::update() {
    controls();
    player_.move(direction(), world_.map.bounds());
    ++frame_;
    rig_.update(player_.box()); // after moving the player: this frame's position
    rig_.apply();
    if (mode_ == Mode::Two) {
        second_.update(world::auto_target(frame_));
        second_.apply();
    }
    touch();
    if (nnh::input::pressed(nnh::Button::Select)) nnh::scenes::replace(*demos_.check);
    if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop();
}

void CameraScene::draw_screen(Screen screen) const {
    gfx::draw_tilemap(screen, world_.map, {.layer = 0});
    const nnh::Vec2 second = world::auto_target(frame_);
    gfx::fill_rect(screen, {second.x, second.y, world::kSquare, world::kSquare}, kSecond, {.layer = 2});
    gfx::fill_rect(screen, player_.box(), kPlayer, {.layer = 3});
    if (has_marker_) {
        gfx::draw_line(screen, marker_ + nnh::Vec2{-6, 0}, marker_ + nnh::Vec2{6, 0}, kMarker, {.layer = 4});
        gfx::draw_line(screen, marker_ + nnh::Vec2{0, -6}, marker_ + nnh::Vec2{0, 6}, kMarker, {.layer = 4});
    }
}

void CameraScene::draw() {
    draw_screen(Screen::Top);
    if (mode_ != Mode::Single) draw_screen(Screen::Bottom);
    rig_.debug_draw();
    if (mode_ == Mode::Two) second_.debug_draw();
    draw_hud(*this);
}
