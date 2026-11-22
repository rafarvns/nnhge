#pragma once
// Counts calls to the global operator new while enabled (alloc_counter.cpp replaces it for the whole test
// binary), so tests can prove that code allocates nothing on the C++ heap. Engine allocations through
// nnh::mem are checked with mem::live_allocations() instead.

namespace nnh::test::alloc_counter {

void enable(bool on); ///< start/stop counting (off by default)
void reset();         ///< count = 0
int count();          ///< operator new calls while enabled since the last reset

} // namespace nnh::test::alloc_counter
