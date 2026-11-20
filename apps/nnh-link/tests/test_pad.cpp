// The number pad (apps/nnh-link/src/pad.cpp): touches and D-Pad focus.

#include "pad.hpp"
#include "test.hpp"

using link_app::Pad;

namespace {

/// The centre of the key cell at (column, row).
int cx(int column) { return Pad::kLeft + column * Pad::kKeyWidth + Pad::kKeyWidth / 2; }
int cy(int row) { return Pad::kTop + row * Pad::kKeyHeight + Pad::kKeyHeight / 2; }

} // namespace

NNH_TEST(pad_touches_hit_the_keys) {
    const Pad pad(true);
    CHECK(pad.hit(cx(0), cy(0)) == '1');
    CHECK(pad.hit(cx(2), cy(2)) == '9');
    CHECK(pad.hit(cx(1), cy(3)) == '0');
    CHECK(pad.hit(cx(3), cy(0)) == Pad::kErase);
    CHECK(pad.hit(cx(3), cy(1)) == '.');
    CHECK(pad.hit(cx(3), cy(2)) == Pad::kOk);
    CHECK(pad.hit(cx(3), cy(3)) == Pad::kOk); // two rows tall
}

NNH_TEST(pad_touches_outside_the_keys_hit_nothing) {
    const Pad pad(true);
    CHECK(pad.hit(Pad::kLeft - 1, cy(0)) == 0);
    CHECK(pad.hit(cx(0), Pad::kTop - 1) == 0);
    CHECK(pad.hit(cx(0), cy(3)) == 0); // the gap left of 0
    CHECK(pad.hit(Pad::kLeft + 4 * Pad::kKeyWidth, cy(0)) == 0);
    CHECK(pad.hit(cx(0), Pad::kTop + 4 * Pad::kKeyHeight) == 0);
}

NNH_TEST(pad_without_the_dot_skips_it) {
    Pad pad(false);
    CHECK(pad.hit(cx(3), cy(1)) == 0);
    pad.move(1, 0);
    pad.move(1, 0);
    pad.move(1, 0); // on '<'
    CHECK(pad.focused() == Pad::kErase);
    pad.move(0, 1); // over the '.' to OK
    CHECK(pad.focused() == Pad::kOk);
}

NNH_TEST(pad_focus_moves_and_stops_at_the_edges) {
    Pad pad(true);
    CHECK(pad.focused() == '1');
    pad.move(-1, 0);
    pad.move(0, -1);
    CHECK(pad.focused() == '1');
    pad.move(0, 1);
    pad.move(0, 1);
    CHECK(pad.focused() == '7');
    pad.move(0, 1); // the gap below 7: stays
    CHECK(pad.focused() == '7');
    pad.move(1, 0);
    pad.move(0, 1);
    CHECK(pad.focused() == '0');
    pad.move(1, 0); // skips the gap to OK
    CHECK(pad.focused() == Pad::kOk);
    pad.move(0, -1); // from the top of OK
    CHECK(pad.focused() == '.');
    pad.move(0, 1);
    pad.move(0, 1); // OK is tall: leaves from its bottom, nothing below
    CHECK(pad.focused() == Pad::kOk);
}
