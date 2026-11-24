// The self-check's steps (check_script.hpp): "at frame f, call ...", over 1200 frames. Requests are made from the
// driver's draw, so each transition starts on the next frame. A and B take turns on top (each replace leaves the
// other); C is pushed and popped.

#include "check_script.hpp"

namespace tr = nnh::transition;
using nnh::Ease;
using nnh::Frames;
using nnh::Screen;

namespace {
enum Op { Replace, Push, Pop, Cover };
} // namespace

void TransitionsCheck::start(int op, nnh::Scene* target, const tr::Options& options) {
    tr::Options o = options;
    if (!o.on_covered) o.on_covered = [this] { note(Note::Covered, 0); };
    if (!o.on_finished) o.on_finished = [this] { note(Note::Finished, 0); };
    bool started = false;
    switch (op) {
    case Replace: started = tr::replace(*target, o); break;
    case Push: started = tr::push(*target, o); break;
    case Pop: started = tr::pop(o); break;
    default: started = tr::cover(o); break;
    }
    if (!started) {
        note(Note::Refused, op);
        return;
    }
    current_ = o;
    if (op == Replace) top_ = 1 - top_;
}

void TransitionsCheck::run_steps() {
    const nnh::Color white = nnh::colors::white;
    nnh::Scene* other = &scene(1 - top_);
    switch (frame_) {
    case 10: start(Replace, other, {.effect = tr::fade()}); break;
    case 70:
        start(Push, &c_, {.effect = tr::wipe(tr::Edge::Left), .out = Frames{20}, .in = Frames{20},
                          .out_ease = Ease::QuadOut, .in_ease = Ease::QuadIn});
        break;
    case 120:
        start(Pop, nullptr, {.effect = tr::wipe(tr::Edge::Top), .reveal_style = tr::Reveal::Continue,
                             .out = Frames{16}, .in = Frames{16}});
        break;
    case 160:
        start(Replace, other, {.effect = tr::split(), .out = Frames{20}, .hold = Frames{10}, .in = Frames{20}});
        break;
    case 220:
        start(Replace, other, {.effect = tr::split(tr::SplitAxis::TopBottom), .reveal_style = tr::Reveal::Continue,
                               .out = Frames{16}, .in = Frames{16}});
        break;
    case 260: start(Cover, nullptr, {.effect = tr::blinds(tr::Edge::Top, 2), .out = Frames{12}, .in = Frames{12}}); break;
    case 290:
        start(Cover, nullptr, {.effect = tr::blinds(tr::Edge::Left, 32), .reveal_style = tr::Reveal::Continue,
                               .out = Frames{18}, .in = Frames{18}});
        break;
    case 330:
        start(Replace, other, {.effect = tr::iris({{200, 120}}), .on_covered = [this] {
                                   note(Note::Covered, 1);
                                   tr::reveal_at({{40, 200}});
                                   current_.reveal = current_.effect;
                                   current_.reveal.center = tr::IrisCenter{{40, 200}};
                               }});
        break;
    case 340: start(Cover, nullptr, {}); break; // refused: the iris is running
    case 390:
        start(Replace, other, {.effect = tr::iris({{-30, 260}}, tr::IrisShape::Diamond, nnh::colors::black, 16),
                               .reveal = tr::iris({{-30, 260}}, tr::IrisShape::Diamond, white, 16),
                               .out = Frames{20}, .hold = Frames{4}, .in = Frames{20}});
        break;
    case 440:
        start(Cover, nullptr, {.effect = tr::wipe(tr::Edge::Right), .bottom = tr::fade(white), .out = Frames{16},
                               .in = Frames{16}});
        break;
    case 480:
        start(Cover, nullptr, {.effect = tr::iris({}, tr::IrisShape::Circle, nnh::rgb(40, 0, 60), 1),
                               .bottom = tr::none(), .out = Frames{20}, .in = Frames{20}});
        break;
    case 530: start(Cover, nullptr, {.effect = tr::wipe(tr::Edge::Top), .tall = true, .out = Frames{20}, .in = Frames{20}}); break;
    case 575:
        start(Cover, nullptr, {.effect = tr::iris({{160, 120}, Screen::Bottom}), .tall = true, .tall_gap = 24});
        break;
    case 630:
        start(Cover, nullptr,
              {.effect = tr::custom(), .out = Frames{10}, .hold = Frames{12}, .in = Frames{10},
               .custom =
                   [this](const tr::CustomFrame& f) {
                       note(Note::Custom, static_cast<int>(f.screen) * 1000 + f.frame);
                       const float w = static_cast<float>(nnh::screen_width(f.screen));
                       nnh::gfx::fill_rect(f.screen, {0, 0, w * f.coverage, 240}, nnh::rgb(250, 200, 40),
                                           {.layer = 15, .ignore_camera = true});
                   },
               .loading = [this] {
                   ++loading_calls_;
                   note(Note::Loading, loading_calls_);
                   return tr::LoadStep{static_cast<float>(loading_calls_) / 7.0f, loading_calls_ == 7};
               }});
        break;
    case 680: start(Replace, other, {.out = Frames{0}, .in = Frames{0}}); break;
    case 690: start(Replace, other, {.out = Frames{0}, .in = Frames{20}}); break;
    case 720: start(Replace, other, {.out = Frames{20}, .in = Frames{0}}); break;
    case 750:
        start(Cover, nullptr, {.effect = tr::fade(white), .out = Frames{20}, .in = Frames{20},
                               .out_ease = Ease::CubicInOut, .in_ease = Ease::SineIn});
        break;
    case 800:
        start(Cover, nullptr, {.effect = tr::wipe(tr::Edge::Bottom), .out = Frames{20}, .in = Frames{20},
                               .out_ease = Ease::BackOut, .in_ease = Ease::BackOut});
        break;
    case 850:
        start(Cover, nullptr, {.effect = tr::split(), .out = Frames{20}, .in = Frames{20},
                               .out_ease = Ease::BounceOut, .in_ease = Ease::BounceOut});
        break;
    case 900:
        start(Replace, other, {.out = Frames{16}, .in = Frames{16}, .update_out = true, .update_in = true});
        break;
    case 940: start(Cover, nullptr, {.out = Frames{10}, .hold = Frames{20}, .in = Frames{10}}); break;
    case 955: nnh::scenes::push(c_); break; // plain changes during the covered phase
    case 965: nnh::scenes::pop(); break;
    case 1000:
        start(Replace, other, {.effect = tr::blinds(tr::Edge::Bottom, 16), .out = Frames{30}, .hold = Frames{5},
                               .in = Frames{30}, .out_ease = Ease::ElasticOut});
        break;
    case 1080:
        start(Cover, nullptr, {.effect = tr::iris({{0, 0}}, tr::IrisShape::Diamond, nnh::colors::black, 2),
                               .out = Frames{30}, .in = Frames{30}, .out_ease = Ease::BounceInOut,
                               .in_ease = Ease::BounceInOut});
        break;
    default: break;
    }
}
