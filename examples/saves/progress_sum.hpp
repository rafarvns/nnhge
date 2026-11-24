#pragma once
// A checksum of a Progress's values (spec 027 AC-2): FNV-1a 32 over every member in id order — strings to
// their length, lists to their count, floats and doubles as their bits — never over struct bytes (padding and
// the alignment of 64-bit members differ between i686, x86-64 and ARM). The golden sums of the check mode and
// the tests.

#include "progress.hpp"

#include <cstdint>

namespace saves_example {

std::uint32_t progress_sum(const Progress& p);

} // namespace saves_example
