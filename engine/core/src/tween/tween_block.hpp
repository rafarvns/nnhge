#pragma once
// The storage of a tween set (plan 015 "Data and memory"): one block [TweenBlock][SlotHot × n][SlotCold × n]
// [Step × s]. The hot part is what update() touches every frame; the cold part (callbacks, event) is read
// only on loop or finish.

#include "nnh/tween.hpp"
#include "scene/scene_attachments.hpp"

#include <cstdint>

namespace nnh::detail {

enum class SlotState : std::uint8_t { Free, Active, Dead };

/// SlotHot::flags.
namespace slot_flag {
inline constexpr std::uint8_t kPaused = 1;
inline constexpr std::uint8_t kStarted = 2;   ///< the delay is over and the start value captured
inline constexpr std::uint8_t kBackwards = 4; ///< yoyo: the current loop plays backwards
inline constexpr std::uint8_t kYoyo = 8;
inline constexpr std::uint8_t kRelative = 16; ///< `by`: to[] holds the delta until the start
inline constexpr std::uint8_t kHasFrom = 32;  ///< `from_to`: nothing to capture
inline constexpr std::uint8_t kFresh = 64;    ///< sequence: the start of the current loop is not played yet
} // namespace slot_flag

/// Step::flags.
namespace step_flag {
inline constexpr std::uint8_t kJoin = 1; ///< plays together with the previous step (same group)
inline constexpr std::uint8_t kCall = 2; ///< a call step: runs in both directions
} // namespace step_flag

struct SlotHot {
    std::uint32_t generation = 1; ///< bumped when the tween ends: its handles go stale at once
    SlotState state = SlotState::Free;
    TweenKind kind = TweenKind::Timer;
    std::uint8_t flags = 0;
    Ease ease = Ease::Linear;
    std::int16_t prev = -1; ///< start-order list (Active and Dead slots); free list uses `next` only
    std::int16_t next = -1;
    std::int16_t first_step = -1; ///< sequence: its first step record. Its steps' children are off the list
    std::int32_t delay = 0;       ///< ticks left before the start
    std::int32_t duration = 0;   ///< ticks of one loop
    std::int32_t elapsed = 0;    ///< ticks into the current loop
    std::int32_t loops_left = 1; ///< including the current loop; kForever
    float from[4] = {};
    float to[4] = {};
    void* target = nullptr; ///< pointer target (null: setter or timer)
    EaseFn custom = nullptr;
    SetterFn setter{};
};

struct SlotCold {
    TweenCallback on_finish{};
    TweenCallback on_loop{};
    FinishEvent publish{};
};

/// A sequence step record (spec 015 FR-10): its child slot (a tween, a wait or a call) and the next step.
struct Step {
    std::int16_t child = -1;
    std::int16_t next = -1; ///< next step of the sequence, or of the free list
    std::uint8_t flags = 0;
    std::uint8_t reserved[3] = {};
};

static_assert(sizeof(SlotHot) + sizeof(SlotCold) <= (sizeof(void*) == 4 ? 192u : 256u),
              "tween slot over its documented size (plan 015: ≈ 184 B on 3DS)");
static_assert(sizeof(Step) == 8);

struct TweenBlock {
    SlotHot* hot = nullptr;
    SlotCold* cold = nullptr;
    Step* steps = nullptr;
    std::int32_t capacity = 0;
    std::int32_t step_capacity = 0;
    std::int32_t speed_ticks = FrameTime::kTicksPerFrame; ///< ticks added per update (speed × 4096)
    std::int32_t remainder = 0;                           ///< advance(): sub-tick carry, in 1/4096 tick
    std::int32_t active = 0;                              ///< Active slots
    std::int32_t dead = 0;                                ///< Dead slots waiting for the sweep
    std::int16_t head = -1;
    std::int16_t tail = -1;
    std::int16_t free_head = -1;
    std::int16_t step_free_head = -1;
    std::uint16_t serial = 0;
    std::uint8_t busy = 0;           ///< > 0 inside an update or a completion: kills only mark slots dead
    std::uint8_t callback_depth = 0; ///< callbacks running on the stack (FR-12)
    bool depth_logged = false;       ///< the depth-limit error is logged once per busy section
    bool doomed = false; ///< its set was destroyed while busy (misuse): freed by the outermost leave
    SceneAttachment attachment{}; ///< bound sets (D30): the node in the scene's list
    Scene* scene = nullptr;       ///< the scene a bound set belongs to
};

} // namespace nnh::detail
