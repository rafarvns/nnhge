#pragma once
// The storage of an action set (plan 017 "Data and memory"): one block [ActionBlock][ActionRec x n][file buffer].
// Plain data, no pointers into other blocks except a bound context's scene.

#include "core/input_hooks.hpp"
#include "nnh/actions.hpp"
#include "scene/scene_attachments.hpp"

#include <cstdint>

namespace nnh::detail {

/// A source as stored: kind, its 4 data bytes (Source::a..d). Touch rectangles live in SlotRec::rect.
struct PackedSource {
    Source::Kind kind = Source::Kind::None;
    std::uint8_t d[4] = {};
    friend constexpr bool operator==(const PackedSource&, const PackedSource&) = default;
};

/// SlotRec::flags (declaration).
namespace slot_flag {
inline constexpr std::uint8_t kLocked = 1;
inline constexpr std::uint8_t kFallback = 2;
inline constexpr std::uint8_t kActive = 4;    ///< usable on this system (computed when the slot changes)
inline constexpr std::uint8_t kInvertX = 8;   ///< current
inline constexpr std::uint8_t kInvertY = 16;  ///< current
inline constexpr std::uint8_t kDefInvertX = 32;
inline constexpr std::uint8_t kDefInvertY = 64;
} // namespace slot_flag

/// SlotRec::state (runtime).
namespace slot_state {
inline constexpr std::uint8_t kBlocked = 1; ///< held when its context was enabled: ignored until released
inline constexpr std::uint8_t kHeld = 2;    ///< stick direction hysteresis
} // namespace slot_state

struct SlotRec {
    PackedSource cur;
    PackedSource def;
    std::uint8_t press = 50, release = 40, dead_zone = 0; ///< percent
    std::uint8_t flags = 0;
    std::uint8_t state = 0;
    Capability fallback_for = Capability::CStick;
    std::uint16_t rect[4] = {}; ///< touch: x, y, w, h
};

/// ActionRec::flags.
namespace action_flag {
inline constexpr std::uint8_t kDeclared = 1;
inline constexpr std::uint8_t kHeld = 2;
inline constexpr std::uint8_t kPressed = 4;
inline constexpr std::uint8_t kReleased = 8;
inline constexpr std::uint8_t kRepeated = 16;
} // namespace action_flag

struct ActionRec {
    char name[kMaxActionName + 1] = {};
    std::uint32_t hash = 0;
    ActionKind kind = ActionKind::Button;
    std::uint8_t context = 0;
    std::uint8_t flags = 0;
    std::uint8_t press = 50, release = 40; ///< axis/vector held threshold, percent
    Repeat repeat{};
    Vec2 out{};                          ///< vector; button/axis: (value, 0)
    float value = 0;                     ///< button 0..1, axis -1..1, vector: its length
    std::uint32_t held_frames = 0;
    std::uint32_t since_pressed = 0xFFFFFFFFu;
    SlotRec slots[kMaxSlots];
};
static_assert(sizeof(ActionRec) <= 160, "action record over its documented size (plan 017: <= 160 B)");

/// ContextRec::flags.
namespace context_flag {
inline constexpr std::uint8_t kDeclared = 1;
inline constexpr std::uint8_t kEnabled = 2; ///< by hand (unbound contexts)
inline constexpr std::uint8_t kOn = 4;      ///< effective state at the last evaluation
inline constexpr std::uint8_t kWarned = 8;  ///< the "unusable action" warning was logged
} // namespace context_flag

struct ActionBlock;

struct ContextRec {
    char name[kMaxContextName + 1] = {};
    std::uint8_t flags = 0;
    Scene* scene = nullptr;    ///< bound: enabled while it is the top scene
    SceneAttachment node{};    ///< D30: ends the binding after the scene's on_exit()
    ActionBlock* block = nullptr;
};

/// The listen in progress (FR-16).
struct ListenRec {
    bool active = false;
    std::uint16_t action = 0;
    std::uint8_t slot = 0;
    ListenOptions options{};
    std::uint32_t ignored = 0;    ///< buttons held at the start, forgotten when released
    std::uint8_t ignored_dirs = 0; ///< stick directions (bit stick * 4 + direction) past 0.5 at the start
    std::uint16_t frames = 0;
};

struct ActionBlock {
    InputHook hook{}; ///< D32: evaluated once per nnh::frame()
    std::uint16_t serial = 0;
    std::uint8_t capacity = 0;  ///< actions
    std::uint8_t contexts = 0;  ///< capacity
    std::uint8_t declared = 0;  ///< actions declared
    std::uint8_t caps = 0;      ///< capability bits read at creation (1 << Capability)
    bool events = true;
    std::uint8_t order[kMaxActions] = {}; ///< declaration order (action indices)
    ContextRec ctx[kMaxContexts];
    ListenRec listen{};
    RemapResult result{};
    ActionRec* actions = nullptr;
    unsigned char* file = nullptr; ///< kMaxBindingsSize bytes for save(path)/load(path)
};

} // namespace nnh::detail
