#include "door_transition.hpp"

namespace tr = nnh::transition;

tr::Options door_transition(nnh::Vec2 centre) {
    tr::Options o;
    o.effect = tr::iris(tr::IrisCenter{centre}, tr::IrisShape::Circle, nnh::colors::black, 4);
    o.tall = true;
    o.out = nnh::Frames{36};
    o.hold = nnh::Frames{6};
    o.in = nnh::Frames{36};
    o.out_ease = nnh::Ease::QuadIn;
    o.in_ease = nnh::Ease::QuadIn;
    return o;
}
