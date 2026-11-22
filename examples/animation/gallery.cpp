#include "gallery.hpp"

#include <cstdio>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

namespace {

const float kSpeeds[] = {0.0f, 0.5f, 1.0f, 1.5f, 2.0f};
constexpr int kSpeedCount = 5;

} // namespace

void Gallery::load() {
    auto texture = nnh::load_texture("art/gallery");
    if (texture) texture_ = std::move(texture.value());
    library_ = make_gallery_library();
    restart();
}

void Gallery::unload() {
    for (nnh::SpriteAnimator& a : cells_) a = nnh::SpriteAnimator{};
    library_ = nnh::SpriteLibrary{};
    texture_.reset();
}

void Gallery::restart() {
    for (int i = 0; i < kGalleryClips; ++i) {
        cells_[i] = nnh::SpriteAnimator{library_};
        cells_[i].play(nnh::SpriteClipId{static_cast<std::uint16_t>(i)});
        cells_[i].set_speed(kSpeeds[speed_]);
    }
}

void Gallery::update() {
    const int before = speed_;
    if (nnh::input::pressed(nnh::Button::L) && speed_ > 0) --speed_;
    if (nnh::input::pressed(nnh::Button::R) && speed_ < kSpeedCount - 1) ++speed_;
    if (nnh::input::pressed(nnh::Button::Y)) restart();
    for (int i = 0; i < kGalleryClips; ++i) {
        if (speed_ != before) cells_[i].set_speed(kSpeeds[speed_]);
        if (flash_[i] > 0) --flash_[i];
        for (const nnh::SpriteNotice& n : cells_[i].update()) {
            if (n.kind == nnh::SpriteNoticeKind::Marker) flash_[i] = 6;
        }
    }
}

void Gallery::draw() const {
    const nnh::Screen top = nnh::Screen::Top;
    char line[64];
    std::snprintf(line, sizeof line, "SPRITE ANIMATION   speed %.1f (L/R)  Y restart", kSpeeds[speed_]);
    nnh::gfx::draw_text(top, {8, 6}, line);
    for (int i = 0; i < kGalleryClips; ++i) {
        const float x = 66.0f + static_cast<float>(i % 3) * 133.0f;
        const float y = 70.0f + static_cast<float>(i / 3) * 105.0f;
        const nnh::SpriteAnimator& a = cells_[i];
        if (flash_[i] > 0) nnh::gfx::fill_rect(top, {x - 28, y - 28, 56, 56}, nnh::rgb(70, 70, 40));
        nnh::DrawOptions o = a.draw_options();
        o.scale = {3, 3};
        nnh::gfx::draw(top, texture_, {x, y}, o);
        std::snprintf(line, sizeof line, "%s\npos %d loops %d%s", kGalleryNames[i], a.position(), a.loops(),
                      a.finished() ? "\nfinished" : "");
        nnh::gfx::draw_text(top, {x, y + 28}, line, {.align = nnh::TextAlign::Center});
    }
}
