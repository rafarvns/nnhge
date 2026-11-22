// Counting global operator new / delete (alloc_counter.hpp). The aligned forms keep the library's own
// versions (nothing in the engine or the tests over-aligns).

#include "alloc_counter.hpp"

#include <cstdlib>
#include <new>

namespace {

std::uint64_t g_news = 0;
std::uint64_t g_deletes = 0;

void* counted_new(std::size_t size) {
    ++g_news;
    void* p = std::malloc(size == 0 ? 1 : size);
    if (p == nullptr) std::abort(); // built without exceptions: no std::bad_alloc
    return p;
}

void counted_delete(void* p) {
    if (p == nullptr) return;
    ++g_deletes;
    std::free(p);
}

} // namespace

namespace alloc_counter {

std::uint64_t news() {
    return g_news;
}
std::uint64_t deletes() {
    return g_deletes;
}

} // namespace alloc_counter

void* operator new(std::size_t size) {
    return counted_new(size);
}
void* operator new[](std::size_t size) {
    return counted_new(size);
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    return counted_new(size);
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    return counted_new(size);
}
void operator delete(void* p) noexcept {
    counted_delete(p);
}
void operator delete[](void* p) noexcept {
    counted_delete(p);
}
void operator delete(void* p, std::size_t) noexcept {
    counted_delete(p);
}
void operator delete[](void* p, std::size_t) noexcept {
    counted_delete(p);
}
void operator delete(void* p, const std::nothrow_t&) noexcept {
    counted_delete(p);
}
void operator delete[](void* p, const std::nothrow_t&) noexcept {
    counted_delete(p);
}
