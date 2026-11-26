#pragma once
// The example's self-check (spec 030 AC-2, AC-6): 600 frames of the world with fixed inputs from the current tuning
// values, folded with FNV-1a over positions, velocities, states, ball positions and every tuning value's bits. The
// defaults' sum and the check script's sum are goldens: the same in the simulator, on every compiler and on the 3DS.
// Pure; also compiled into nnh_tests (tests/live_example/test_self_check.cpp).

#include <cstdint>

#include "sim.hpp"

inline constexpr std::uint32_t kLiveDefaultsGolden = 0x29C765A7u; // recorded at T7.1 from pc-debug
inline constexpr std::uint32_t kLiveScriptGolden = 0x7A849F13u;   // the values tests/check.nnhs sets

/// The sum of 600 frames from `tuning`.
std::uint32_t live_self_check(const Tuning& tuning);

/// The tuning values tests/check.nnhs sets (for the golden test).
Tuning live_script_tuning();
