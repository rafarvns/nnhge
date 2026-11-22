#pragma once
// The data example's self-check (spec 014 AC-1, AC-7, FR-16): loads every data asset of every type, follows every
// reference, checks the errors and that loads allocate nothing, and sums every loaded value field by field
// (never raw struct bytes: the PC and the 3DS lay structs out differently) into a CRC-32. The simulator and the
// console must print the same line: tests/check.nnhs expects it.

#include <cstddef>
#include <cstdint>

struct DataCheckResult {
    bool pass = false;
    std::uint32_t checksum = 0;
    int enemies = 0, items = 0, settings = 0;
    std::size_t index_bytes = 0; ///< nnh::data::memory_used() after the run
    char failure[96] = {};       ///< what failed first ("" on success)
};

/// Runs the whole check (load time: a few dozen loads). Needs nnh::init().
DataCheckResult run_data_check();

/// "data check: PASS checksum 0x... (4 enemies, 3 items, 1 settings, index N bytes)" or "data check: FAIL ...".
void format_data_check(const DataCheckResult& result, char* out, std::size_t size);
