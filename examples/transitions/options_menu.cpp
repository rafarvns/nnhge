#include "options_menu.hpp"

#include "curtain.hpp"

namespace tr = nnh::transition;

const char* fx_name(FxChoice fx) {
    switch (fx) {
    case FxChoice::Fade: return "fade";
    case FxChoice::Wipe: return "wipe";
    case FxChoice::Split: return "split";
    case FxChoice::Blinds: return "blinds";
    case FxChoice::Iris: return "iris";
    case FxChoice::Diamond: return "diamond";
    case FxChoice::Curtain: return "curtain";
    case FxChoice::Count: break;
    }
    return "?";
}

const char* screens_name(ScreensChoice screens) {
    switch (screens) {
    case ScreensChoice::Both: return "both";
    case ScreensChoice::TopOnly: return "top";
    case ScreensChoice::Tall: return "tall";
    case ScreensChoice::Count: break;
    }
    return "?";
}

namespace {

tr::Effect effect_of(const Settings& s) {
    const nnh::Color ink = nnh::rgb(12, 10, 24);
    switch (s.fx) {
    case FxChoice::Wipe: return tr::wipe(s.screens == ScreensChoice::Tall ? tr::Edge::Top : tr::Edge::Left, ink);
    case FxChoice::Split: return tr::split(tr::SplitAxis::LeftRight, ink);
    case FxChoice::Blinds: return tr::blinds(tr::Edge::Top, 8, ink);
    case FxChoice::Iris: return tr::iris(s.centre, tr::IrisShape::Circle, ink);
    case FxChoice::Diamond: return tr::iris(s.centre, tr::IrisShape::Diamond, ink);
    case FxChoice::Curtain: return tr::custom();
    default: return tr::fade(ink);
    }
}

} // namespace

tr::Options make_options(const Settings& s, Curtain& curtain) {
    tr::Options o;
    o.effect = effect_of(s);
    o.reveal_style = s.continue_reveal ? tr::Reveal::Continue : tr::Reveal::Reverse;
    o.tall = s.screens == ScreensChoice::Tall;
    if (s.screens == ScreensChoice::TopOnly) o.bottom = tr::none();
    o.out = nnh::Frames{24};
    o.in = nnh::Frames{24};
    o.out_ease = nnh::Ease::QuadIn;
    o.in_ease = nnh::Ease::QuadIn;
    if (s.fx == FxChoice::Curtain) o.custom = curtain.effect();
    return o;
}
