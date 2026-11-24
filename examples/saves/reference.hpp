#pragma once
// The reference values of the saves example (spec 027 AC-2): one seeded sequence fills each save version, so
// the golden files v1.sav, v2.sav and v3.sav hold the same game at three points in its history. A 12-byte
// name with a two-byte character, 64 items (32 in v1) with one subnormal `wear`, 1024 explored flags, 512
// bestiary counts, 20 quests, a seed and (v2, v3) a play time above 2^32, a best time with a fractional part.

#include "progress_history.hpp"

namespace saves_example {

// Each fills `out` in place (a Progress is about 5 KB: never a temporary on the 3DS main thread's stack).
void fill_reference_v1(ProgressV1& out);
void fill_reference_v2(ProgressV2& out);
void fill_reference_v3(Progress& out);

/// What reference v1 becomes after loading into Progress (v3) with its two steps: the hand-made expectation.
void fill_reference_v1_migrated(Progress& out);

/// The values of reference "B" for the check mode (a second store over the first).
void fill_reference_v3_b(Progress& out);

// By value, for the tests (on the PC's large stack).
inline ProgressV1 reference_v1() {
    ProgressV1 p;
    fill_reference_v1(p);
    return p;
}
inline ProgressV2 reference_v2() {
    ProgressV2 p;
    fill_reference_v2(p);
    return p;
}
inline Progress reference_v3() {
    Progress p;
    fill_reference_v3(p);
    return p;
}
inline Progress reference_v1_migrated() {
    Progress p;
    fill_reference_v1_migrated(p);
    return p;
}

} // namespace saves_example
