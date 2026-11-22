#pragma once
// The actions example's controls (spec 017 AC-7): one set for the whole game, declared once. Play: move (Circle Pad
// or D-Pad), jump (A), attack (B, or the on-screen button), aim (C-Stick X; L/R on an Old 3DS), pause (START,
// locked). Menu: up/down with repeat, confirm A, back B, reset X, reset all Y.

#include <cstdint>
#include <nnh/actions.hpp>

enum class Act : std::uint8_t { Move, Jump, Attack, Aim, Pause, Up, Down, Confirm, Back, Reset, ResetAll, Count };
enum class Ctx : std::uint8_t { Play, Menu, Count };

/// The play actions a player can rebind, in the controls screen's order.
inline constexpr Act kRebindable[] = {Act::Move, Act::Jump, Act::Attack, Act::Aim};

inline constexpr const char* kBindingsPath = "controls.bin";
inline constexpr std::uint32_t kBindingsVersion = 1;

/// The on-screen attack button (bottom screen pixels).
inline constexpr int kTouchX = 232, kTouchY = 168, kTouchW = 80, kTouchH = 64;

/// Declares every context and action. False if a declaration was refused.
bool declare_controls(nnh::ActionSet& set);
