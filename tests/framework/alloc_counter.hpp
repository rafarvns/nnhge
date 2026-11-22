#pragma once
// Counting global operator new / delete for the "allocates nothing" tests (spec 014 AC-7; shared with plans
// 015 and 016, see their notes). alloc_counter.cpp replaces the global operators, so link it into one test
// program only (nnh_tests lists it as a source). Engine memory (nnh::mem::alloc) doesn't go through operator
// new: check nnh::mem::live_allocations() and the tag stats as well.
//
//   const auto before = alloc_counter::news();
//   ... calls under test ...
//   CHECK(alloc_counter::news() == before);

#include <cstdint>

namespace alloc_counter {

/// Calls of the replaced operator new / new[] (plain and nothrow) since the program started.
std::uint64_t news();

/// Calls of the replaced operator delete / delete[] with a non-null pointer.
std::uint64_t deletes();

} // namespace alloc_counter
