#pragma once
/// @file nnh/actions.hpp
/// Input actions (spec 017): named actions ("jump", "move") bound to controls, grouped in contexts that can
/// follow scenes, read as queries or events, remapped by the player and saved. Optional: a game that never
/// creates an ActionSet links none of it and keeps reading <nnh/input.hpp>. See docs/api/actions.md.
///
/// @code
/// enum class Act { Jump, Move };
/// nnh::ActionSet controls{{.actions = 2}};                       // one allocation (tag "actions")
/// controls.add(Act::Jump, {.name = "jump", .bindings = {{nnh::Source::button(nnh::Button::A)}}});
/// controls.add(Act::Move, {.name = "move", .kind = nnh::ActionKind::Vector,
///                          .bindings = {{nnh::Source::stick(nnh::AnalogStick::CirclePad)},
///                                       {nnh::Source::dpad()}}});
/// if (controls.pressed(Act::Jump)) jump();                         // evaluated by nnh::frame()
/// player.x += controls.vector(Act::Move).x * kSpeed;
/// @endcode
/// Sets are evaluated automatically once per nnh::frame(), right after the input sample (decision D32).
/// Nothing allocates after construction; not thread-safe (use from the game's thread).

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "nnh/gfx.hpp"
#include "nnh/input.hpp"
#include "nnh/math.hpp"
#include "nnh/result.hpp"
#include "nnh/system.hpp"

namespace nnh {

class Scene;
namespace detail {
struct ActionBlock;
} // namespace detail

enum class ActionKind : std::uint8_t {
    Button, ///< digital: held, pressed, released; value 0..1
    Axis,   ///< one float in [-1, 1]
    Vector, ///< 2D (x right, y up), length <= 1
};
enum class AnalogStick : std::uint8_t { CirclePad, CStick };
enum class Direction : std::uint8_t { Up, Down, Left, Right };
enum class StickAxis : std::uint8_t { X, Y };
/// Which part of a slot: the whole source, or one button of a composite (axis: Negative/Positive; vector:
/// Up/Down/Left/Right).
enum class Part : std::uint8_t { Whole, Negative, Positive, Up, Down, Left, Right };

/// A game enum value used as an action or context id: values 0..capacity-1.
struct ActionId {
    template <typename E>
        requires std::is_enum_v<E>
    constexpr ActionId(E e) : index(static_cast<std::uint16_t>(e)) {}
    constexpr explicit ActionId(int i) : index(static_cast<std::uint16_t>(i)) {}
    std::uint16_t index;
};
using ContextId = ActionId;

namespace detail {
/// Bit index (0..13) of a Button value; 0xFF for a value that is not one button.
constexpr std::uint8_t button_index(Button b) {
    const auto v = static_cast<std::uint32_t>(b);
    for (std::uint8_t i = 0; i < 14; ++i) {
        if (v == (1u << i)) return i;
    }
    return 0xFF;
}
} // namespace detail

/// What a binding slot reads (FR-6). A small value type; build it with the factories.
struct Source {
    enum class Kind : std::uint8_t { None, Button, StickDirection, Stick, StickAxis, Buttons2, Buttons4, Touch };
    /// An unbound part of a composite.
    static constexpr std::uint8_t kNoButton = 0xFF;

    Kind kind = Kind::None;
    std::uint8_t a = 0, b = 0, c = 0, d = 0;  ///< button bit indices, stick, direction / axis
    std::uint16_t x = 0, y = 0, w = 0, h = 0; ///< touch rectangle (bottom screen pixels)

    /// One button (button actions).
    static constexpr Source button(Button btn) { return {Kind::Button, detail::button_index(btn)}; }
    /// A stick pushed one way: button actions (held past 0.5, let go below 0.4) and axis actions (0..1).
    static constexpr Source stick_direction(AnalogStick s, Direction dir) {
        return {Kind::StickDirection, static_cast<std::uint8_t>(s), static_cast<std::uint8_t>(dir)};
    }
    /// A whole stick (vector actions).
    static constexpr Source stick(AnalogStick s) { return {Kind::Stick, static_cast<std::uint8_t>(s)}; }
    /// One axis of a stick (axis actions).
    static constexpr Source stick_axis(AnalogStick s, StickAxis axis) {
        return {Kind::StickAxis, static_cast<std::uint8_t>(s), static_cast<std::uint8_t>(axis)};
    }
    /// Two buttons for an axis: value = positive - negative.
    static constexpr Source buttons(Button negative, Button positive) {
        return {Kind::Buttons2, detail::button_index(negative), detail::button_index(positive)};
    }
    /// Four buttons for a vector; opposites cancel, a diagonal has length 1.
    static constexpr Source buttons(Button up, Button down, Button left, Button right) {
        return {Kind::Buttons4, detail::button_index(up), detail::button_index(down), detail::button_index(left),
                detail::button_index(right)};
    }
    /// The D-Pad as a vector composite.
    static constexpr Source dpad() { return buttons(Button::Up, Button::Down, Button::Left, Button::Right); }
    /// A rectangle of the bottom screen (button actions): held while touched inside. Always locked.
    static constexpr Source touch(int px, int py, int pw, int ph) {
        return {Kind::Touch, 0, 0, 0, 0, static_cast<std::uint16_t>(px), static_cast<std::uint16_t>(py),
                static_cast<std::uint16_t>(pw), static_cast<std::uint16_t>(ph)};
    }
    friend constexpr bool operator==(const Source&, const Source&) = default;
};

/// One binding slot as declared (FR-6, FR-8, FR-9). Thresholds and the dead zone are kept in 1 % steps.
struct Binding {
    Source source{};
    float press = 0.5f, release = 0.4f; ///< stick directions in button actions
    float dead_zone = 0.0f;             ///< extra dead zone of a whole stick / stick axis (re-scaled)
    bool invert_x = false, invert_y = false;
    bool locked = false;   ///< never changed by remapping, reset or loading (touch areas always are)
    bool fallback = false; ///< used only on systems without `fallback_for`
    Capability fallback_for = Capability::CStick;
};

/// Menu repeat in frames (FR-12): repeated() on the press frame, after `delay`, then every `interval`.
struct Repeat {
    std::uint8_t delay = 0, interval = 0;
};

inline constexpr int kMaxActions = 64, kMaxSlots = 4, kMaxContexts = 8, kMaxActionName = 31;
inline constexpr int kMaxContextName = 15;

/// One action's declaration (designated initializers).
struct ActionDesc {
    const char* name = nullptr; ///< 1-31 characters of [a-z0-9_], copied
    ActionKind kind = ActionKind::Button;
    ContextId context = ActionId(0);
    Binding bindings[kMaxSlots] = {};
    Repeat repeat{};
    float press = 0.5f, release = 0.4f; ///< held threshold of axis/vector actions (FR-3)
};

struct ActionSetConfig {
    int actions = 0;  ///< required: 1..kMaxActions (ids 0..actions-1)
    int contexts = 1; ///< 1..kMaxContexts (ids 0..contexts-1)
};

enum class ConflictPolicy : std::uint8_t { Swap, Replace, Allow, Reject };
enum class ListenState : std::uint8_t { Idle, Listening, Bound, Cancelled, TimedOut, Rejected };
enum class Resolution : std::uint8_t { None, Swapped, Replaced, Allowed, Rejected };

struct ListenOptions {
    Part part = Part::Whole;
    ConflictPolicy on_conflict = ConflictPolicy::Swap;
    std::uint32_t cancel = static_cast<std::uint32_t>(Button::Start); ///< Button mask; 0 = none
    std::uint32_t exclude = 0;                                         ///< Button mask never captured
    bool sticks = true;                                                ///< sticks may be captured
    std::uint16_t timeout_frames = 0;                                  ///< 0 = none
};

/// What a listen (or set_source/set_part) did. A listen's result stays readable until the next listen() or
/// clear_listen_result().
struct RemapResult {
    ListenState state = ListenState::Idle;
    std::uint16_t action = 0;
    std::uint8_t slot = 0;
    Part part = Part::Whole;
    Source source{}; ///< captured / set
    Resolution resolution = Resolution::None;
    std::uint16_t other_action = 0; ///< the first other binding that shared the source
    std::uint8_t other_slot = 0;
    Part other_part = Part::Whole;
    std::uint8_t conflicts = 0; ///< how many bindings shared it
};

/// Two bindings of one context that read a common control (conflicts()).
struct Conflict {
    std::uint8_t slot;
    Part part;
    std::uint16_t other_action;
    std::uint8_t other_slot;
    Part other_part;
};

struct LoadReport {
    bool found = false;           ///< false: no file (a normal "no remaps")
    bool version_ignored = false; ///< the game's bindings version differed: nothing applied
    int applied = 0, skipped = 0;
};

inline constexpr std::size_t kMaxBindingsSize = 3088; ///< 16 + 256 x 12: the largest bindings blob

/// A game-owned set of actions (FR-1). One heap block (tag "actions") reserved at construction; the object
/// itself is one pointer and can move. Misuse (bad ids, capacities) asserts in Debug/Development and is
/// ignored with an error log in Release. Changes (enable, bind, remap, reset, load) apply from the next
/// evaluation, so queries and events of one frame always agree.
class ActionSet {
public:
    ActionSet() = default; ///< empty: valid() is false, every query reads idle
    explicit ActionSet(const ActionSetConfig& config);
    ~ActionSet(); ///< asserts if a context is still bound to a scene on the stack (detaches in Release)
    ActionSet(ActionSet&& other) noexcept;
    ActionSet& operator=(ActionSet&& other) noexcept;
    ActionSet(const ActionSet&) = delete;
    ActionSet& operator=(const ActionSet&) = delete;

    bool valid() const;
    std::uint16_t id() const; ///< matches ActionPressed::set

    // Declaring (FR-1..FR-9). Context 0 exists as "default"; false (and an assert) on misuse.
    bool add_context(ContextId id, const char* name, bool enabled = true); ///< name: 1-15 of [a-z0-9_]
    bool add(ActionId id, const ActionDesc& desc);

    // Contexts (FR-4, FR-5, FR-13).
    void enable(ContextId id, bool enabled);
    bool enabled(ContextId id) const; ///< bound: whether its scene is on top
    /// Enabled exactly while `scene` is the top scene, until it leaves the stack (call in on_enter()).
    void bind_context(ContextId id, Scene& scene);
    void unbind_context(ContextId id); ///< back to manual, disabled

    // Reading (FR-11, FR-12). Per-frame safe, no allocation.
    bool pressed(ActionId a) const;
    bool held(ActionId a) const;
    bool released(ActionId a) const;
    bool repeated(ActionId a) const;
    float value(ActionId a) const;               ///< button 0..1, axis -1..1, vector: its length
    Vec2 vector(ActionId a) const;               ///< x right, y up, length <= 1 (axis: (value, 0))
    std::uint32_t held_frames(ActionId a) const; ///< 1 on the press frame, 0 when not held
    std::uint32_t frames_since_pressed(ActionId a) const; ///< 0 on the press frame; UINT32_MAX = never
    bool usable(ActionId a) const;                        ///< any active, bound slot on this system (FR-9)
    void set_events_enabled(bool on);                     ///< FR-14, default on

    // Remapping (FR-16..FR-19). No allocation.
    void listen(ActionId a, int slot, const ListenOptions& options = {});
    void stop_listening(); ///< state -> Cancelled
    bool listening() const;
    const RemapResult& listen_result() const;
    void clear_listen_result();
    RemapResult set_source(ActionId a, int slot, Source source, ConflictPolicy policy = ConflictPolicy::Swap);
    RemapResult set_part(ActionId a, int slot, Part part, Button button,
                         ConflictPolicy policy = ConflictPolicy::Swap);
    void clear(ActionId a, int slot);
    void set_invert(ActionId a, int slot, bool invert_x, bool invert_y);
    void reset(ActionId a, int slot);
    void reset(ActionId a);
    void reset_context(ContextId id);
    void reset_all();
    Source source(ActionId a, int slot) const; ///< current; Kind::None if empty
    /// Every binding of `a` that shares a control with another binding of its context; the count (writes at
    /// most `capacity`).
    int conflicts(ActionId a, Conflict* out, int capacity) const;
    /// Its active slots joined with " / " ("A / B"); the length written (truncated, NUL-ended).
    std::size_t describe(ActionId a, char* out, std::size_t capacity) const;
    const char* name(ActionId a) const; ///< "" for an unknown id

    // Saving (FR-20, FR-21): only the player's changes; never asserts on file content.
    Result<void> save(const char* path, std::uint32_t game_version = 0) const;
    Result<LoadReport> load(const char* path, std::uint32_t game_version = 0);
    Result<std::size_t> save_to(void* buffer, std::size_t capacity, std::uint32_t game_version = 0) const;
    Result<LoadReport> load_from(const void* data, std::size_t size, std::uint32_t game_version = 0);

    /// Debug view (FR-24): enabled contexts and held actions as debug text. Nothing in Release.
    void debug_draw(Screen screen, int x, int y) const;

private:
    detail::ActionBlock* block_ = nullptr;
};

/// Published for button actions, and for axis/vector actions when held() changes (FR-14), after the frame's
/// ButtonPressed/ButtonReleased/touch events: per set, presses then releases, each in declaration order.
struct ActionPressed {
    std::uint16_t set;
    std::uint16_t action;
    bool is(const ActionSet& s, ActionId a) const { return set == s.id() && action == a.index; }
};
struct ActionReleased {
    std::uint16_t set;
    std::uint16_t action;
    bool is(const ActionSet& s, ActionId a) const { return set == s.id() && action == a.index; }
};

namespace actions {
/// Short English name of a single control: "A", "ZL", "D-Pad Up", "Circle Pad Left", "C-Stick",
/// "C-Stick X", "Touch"; "" for None. Static strings in the built-in font's characters.
const char* control_name(Source source);
/// Any source, composites included ("D-Pad", "L/R", "X/B/Y/A"); the length written (truncated, NUL-ended).
std::size_t describe(Source source, char* out, std::size_t capacity);
} // namespace actions

} // namespace nnh
