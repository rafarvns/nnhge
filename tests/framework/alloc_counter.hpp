#pragma once
// Counting global operator new / delete for the "allocates nothing" tests (spec 014 AC-7, spec 015, plan 016).
// alloc_counter.cpp replaces the global operators, so link it into one test program only (nnh_test_framework).
// Engine memory (nnh::mem::alloc) doesn't go through operator new: check nnh::mem::live_allocations() and the
// tag stats as well. Two styles, same counter:
//
//   const auto before = alloc_counter::news();          // always counting
//   ... calls under test ...
//   CHECK(alloc_counter::news() == before);
//
//   nnh::test::alloc_counter::reset(); nnh::test::alloc_counter::enable(true);   // counting window
//   ... calls under test ...
//   nnh::test::alloc_counter::enable(false); CHECK(nnh::test::alloc_counter::count() == 0);

#include <cstdint>

namespace alloc_counter {

/// Calls of the replaced operator new / new[] (plain and nothrow) since the program started.
std::uint64_t news();

/// Calls of the replaced operator delete / delete[] with a non-null pointer.
std::uint64_t deletes();

} // namespace alloc_counter

namespace nnh::test::alloc_counter {

void enable(bool on); ///< start/stop counting (off by default)
void reset();         ///< count = 0
int count();          ///< operator new calls while enabled since the last reset

} // namespace nnh::test::alloc_counter
