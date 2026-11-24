#pragma once
// The reference values of the saves example (spec 027 AC-2): one seeded sequence fills each save version, so the
// golden files v1.sav, v2.sav and v3.sav hold the same game at three points in its history. A 12-byte name with a
// two-byte character, 64 items (32 in v1) with one subnormal `wear`, 1024 explored flags, 512 bestiary counts, 20
// quests, a seed and (v2, v3) a play time above 2^32, a best time with a fractional part.

#include "progress_history.hpp"

namespace saves_example {

ProgressV1 reference_v1();
ProgressV2 reference_v2();
Progress reference_v3();

/// What reference_v1() becomes after loading into Progress (v3) with its two steps: the hand-made expectation.
Progress reference_v1_migrated();

/// The values of reference "B" for the check mode (a second store over the first).
Progress reference_v3_b();

} // namespace saves_example
