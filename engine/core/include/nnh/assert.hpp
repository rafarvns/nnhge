#pragma once
/// @file nnh/assert.hpp
/// Catching bugs: NNH_ASSERT, NNH_VERIFY, NNH_PANIC. A failure logs at `fatal`, shows the crash screen
/// on both screens and stops the game. See docs/api/assert.md and docs/architecture/errors.md.
///
/// @code
/// NNH_ASSERT(index < count, "index %d out of range (%d)", index, count); // Debug + Development
/// NNH_VERIFY(save != nullptr, "save slot missing");                     // every flavor
/// NNH_PANIC("unknown state %d", state);                                   // never returns
/// @endcode
/// The message is optional: `NNH_ASSERT(ptr)`. In Release, NNH_ASSERT compiles to nothing and its
/// condition is **not evaluated** — never put side effects in it.

#include "nnh/build.hpp"
#include "nnh/detail/printf_format.hpp"

namespace nnh::detail {

enum class FailKind { Assert, Verify, Panic };

[[noreturn]] void fail(FailKind kind, const char* expression, const char* file, int line, const char* function,
                       const char* format, ...) NNH_PRINTF_FORMAT(6, 7);

} // namespace nnh::detail

// The message is optional: `"" __VA_ARGS__` turns a missing message into an empty format string, which
// GCC flags with -Wformat-zero-length. The macros below silence that one warning around their own
// statement (pragmas must sit at statement level, so they wrap the whole do/while block).
#if defined(__GNUC__)
#define NNH_DETAIL_DIAG_PUSH _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wformat-zero-length\"")
#define NNH_DETAIL_DIAG_POP _Pragma("GCC diagnostic pop")
#else
#define NNH_DETAIL_DIAG_PUSH
#define NNH_DETAIL_DIAG_POP
#endif

#define NNH_DETAIL_FAIL(kind, expr, ...)                                                                      \
    ::nnh::detail::fail(::nnh::detail::FailKind::kind, expr, __FILE__, __LINE__, __func__, "" __VA_ARGS__)

/// Checks a condition in every flavor; on failure shows the crash screen.
#define NNH_VERIFY(cond, ...)                                                                                 \
    do {                                                                                                      \
        NNH_DETAIL_DIAG_PUSH                                                                                  \
        if (!(cond)) [[unlikely]] {                                                                           \
            NNH_DETAIL_FAIL(Verify, #cond, __VA_ARGS__);                                                      \
        }                                                                                                     \
        NNH_DETAIL_DIAG_POP                                                                                   \
    } while (0)

#if defined(NNH_FLAVOR_RELEASE)
/// Checks a condition in Debug and Development; compiled out (not evaluated) in Release.
#define NNH_ASSERT(cond, ...)                                                                                 \
    do {                                                                                                      \
        (void)sizeof(!(cond));                                                                                \
    } while (0)
#else
#define NNH_ASSERT(cond, ...)                                                                                 \
    do {                                                                                                      \
        NNH_DETAIL_DIAG_PUSH                                                                                  \
        if (!(cond)) [[unlikely]] {                                                                           \
            NNH_DETAIL_FAIL(Assert, #cond, __VA_ARGS__);                                                      \
        }                                                                                                     \
        NNH_DETAIL_DIAG_POP                                                                                   \
    } while (0)
#endif

/// Marks a state that must never happen; always shows the crash screen.
#define NNH_PANIC(...)                                                                                        \
    do {                                                                                                      \
        NNH_DETAIL_DIAG_PUSH                                                                                  \
        NNH_DETAIL_FAIL(Panic, nullptr, __VA_ARGS__);                                                         \
        NNH_DETAIL_DIAG_POP                                                                                   \
    } while (0)
