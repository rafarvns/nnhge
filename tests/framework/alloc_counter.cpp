// A counting replacement of the global operator new/delete (one definition per test binary). The array and
// nothrow forms of the standard library forward to these.

#include "alloc_counter.hpp"

#include <cstdlib>
#include <new>

namespace {
bool g_enabled = false;
int g_count = 0;
} // namespace

void nnh::test::alloc_counter::enable(bool on) { g_enabled = on; }
void nnh::test::alloc_counter::reset() { g_count = 0; }
int nnh::test::alloc_counter::count() { return g_count; }

void* operator new(std::size_t size) {
    if (g_enabled) ++g_count;
    void* p = std::malloc(size == 0 ? 1 : size);
    if (p == nullptr) std::abort(); // tests are built without exceptions in mind: fail loudly
    return p;
}

void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
