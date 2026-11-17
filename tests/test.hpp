#pragma once
// Minimal test helpers. A test is a function registered with NNH_TEST; CHECK records failures.

#include <cstdio>

namespace nnh::test {

using TestFn = void (*)();
void register_test(const char* name, TestFn fn);
void fail(const char* file, int line, const char* expr);

struct Registrar {
    Registrar(const char* name, TestFn fn) { register_test(name, fn); }
};

} // namespace nnh::test

#define NNH_TEST(name)                                                                                     \
    static void name();                                                                                    \
    static ::nnh::test::Registrar name##_registrar(#name, name);                                           \
    static void name()

#define CHECK(expr)                                                                                        \
    do {                                                                                                   \
        if (!(expr)) ::nnh::test::fail(__FILE__, __LINE__, #expr);                                         \
    } while (0)
