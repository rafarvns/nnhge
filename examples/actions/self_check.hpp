#pragma once
// The actions example's self-check (spec 017 AC-8): the console script rebinds jump to B (a swap with attack),
// resets it, rebinds it to Y, leaves the controls screen (save), presses Y and A in play, reloads the bindings
// and runs the bench. This records each step as it happens and gives the verdict line. Pure (no engine calls), so
// tests/actions_example/test_self_check.cpp runs it.

#include <cstddef>
#include <cstdint>
#include <nnh/actions.hpp>

class ActionsSelfCheck {
public:
    /// A listen ended with this result (names of the action and of the other action).
    void remap(const char* action, nnh::Source source, nnh::Resolution resolution, const char* other);
    /// The bindings were saved; `sum` is their checksum.
    void saved(std::uint32_t sum);
    /// An action was pressed in play.
    void pressed(const char* action);
    /// The bindings were loaded again from the file.
    void reloaded(std::uint32_t sum, int applied, int skipped);
    /// The bench ended: average ms per frame (negative: not measured). On the console it must be under 0.1 ms.
    void bench(float avg_ms, bool console);

    bool done() const { return done_; }
    bool passed() const { return steps_ == kAll; }
    /// "actions: PASS 7/7" or "actions: FAIL <the first missing step>".
    void verdict(char* out, std::size_t size) const;

private:
    enum : std::uint8_t {
        kSwap = 1,
        kReboundY = 2,
        kSaved = 4,
        kJump = 8,
        kAttack = 16,
        kReload = 32,
        kBench = 64,
        kAll = 127
    };
    std::uint8_t steps_ = 0;
    std::uint32_t saved_sum_ = 0;
    bool done_ = false;
};
