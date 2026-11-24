# Plan 022: Scene tree and transforms

Status: approved 2026-11-23 (overnight pre-approval)
Spec: [spec.md](spec.md) · Tasks: [tasks.md](tasks.md) · Decision D37 ([decisions.md](../../docs/architecture/decisions.md))
Uses spec 020 (worlds, kinds and remove hooks, handles, the destroy queue, quarantine, iteration guards, scene binding;
D35), spec 021 (prefab libraries, the all-or-nothing reservation, placement, hooks, map objects; D36), spec 015 (the
libm-free `sin_quarter`), spec 004 (`DrawOptions`), spec 018 (`CameraRig`), spec 019 (box views, `boxes_at`), spec 002
(asserts, logs, `Result`, tagged memory, profiler), spec 013 (scripts)

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ⚠️→✅ | Pressure point: "the same bits on PC and 3DS" now covers sine and cosine. Answered by spec 015's polynomial sine (moved where both modules share it, bits unchanged), a range reduction made only of `×`, `−` and float↔int conversions, every transform float operation in core sources compiled with `-ffp-contract=off` (no float arithmetic inline in a public header, where a game's flags would apply), integer bit tests instead of float compares for signs and finiteness, and AC-2's golden on every host compiler and the console. Two documented limits: the 3DS flushes subnormals to zero (the PC doesn't), so values whose products fall below 2⁻¹²⁶ may differ — the self-check's domain has none and the inverse's threshold is an exponent test; 32-bit x86 hosts (x87) are not supported (already true since spec 015) |
| II 3DS constraint | ⚠️→✅ | Pressure point: AC-7's five targets predate any measurement and specs 019–021 showed estimates 1.3–2× low. Answered by the ARM11 cost plan below: an 80-byte node record ordered by use, a pass that walks only the node storage, depth-first through parent links with no stack and a prefetch two nodes ahead, no out-of-line call per node (the sine only when a rotation changed, inline), no `vmrs`/`__aeabi_*`/`memcpy` in hot paths, keep-world reparents that need no sine and no chain walk, and a disassembly check before the console (T7.6). Two targets are at risk (pass with positions only, reparents) and one is tight (`world_now`); AC-7's measurement-with-margin rule applies. Memory: 80 B per node (≤ 96, the spec's expectation), 24 B per world, nothing per entity without a node |
| III Build speed | ✅ | `<nnh/transform.hpp>` includes only `<nnh/math.hpp>` (declarations, no templates); `<nnh/hierarchy.hpp>` = `<nnh/entities.hpp>` + transform + one detail header, templates only for `each_descendant` and the hooked `node_component`; `gfx.hpp` and `prefabs.hpp` gain declarations only (forward declaration, transform header). Measured before/after (T0.1, T11.4) |
| IV Event-driven | ✅ | No events (FR-20): state is queried (`parent`, `children`, `world_transform`) |
| V No hidden costs | ✅ | No exceptions (`bool`, `AttachCheck`, `Result` at registration), no RTTI (spec 020's kind ids), no `virtual`, no allocation after the world's or library's creation; the pass calls no game code |
| VI Spec + test first | ✅ | Pure logic first on the fake platform: transform math against a `double` reference, the hierarchy against spec 020's own calls, AC-2's twin-world golden (prefab side vs hand side); the platformer's replay lines regenerated deliberately (decision 24) and then required on the console |
| VII Small files | ✅ | Two modules (transform: 6 sources; hierarchy: 10 sources), one public header each; entities, prefabs and gfx gain one source each; all planned ≤ 250 lines |
| VIII Documented API | ✅ | `docs/api/transform.md`, `docs/api/hierarchy.md`, links from math, entities, prefabs, gfx, camera; the levels guide; `examples/hierarchy`; the platformer; D37 |
| IX Clean and legal | ✅ | Own code from published designs (Godot, Unity, Bevy, flecs, EnTT articles, Bitsquid, *Game Programming Patterns*); cited in `docs/REFERENCES.md`; original art |
| X Safe by default | ✅ | No parser, no file format. Misuse asserts in Debug/Development and is harmless in Release (FR-23): non-finite locals are caught at the pass, angles of any size or NaN give defined bits, no float→int overflow |
| XI Opt-in, never in the way | ⚠️→✅ | Pressure points: spec 020's non-goal "the engine ships no component" and spec 021's "nested prefabs". Answered: the node kind is registered by the game like any kind; spec 020's public header is untouched (additions in `src/entities/entities_internal.hpp`); a prefab without children is stored and spawned as today, and the tree code is linked only through `nnh::child()` (a game that never calls it links no hierarchy code); a game using only `<nnh/transform.hpp>` links no entities code; `hello` unchanged |

## Design

### Overview

```
<nnh/transform.hpp> (only <nnh/math.hpp>)          <nnh/hierarchy.hpp> (<nnh/entities.hpp> + transform)
  Transform2D   local: position, rotation, scale       Node: the engine's one component kind (80 B)
  Affine2D      x axis, y axis, origin                   links (node slots) · local · sine cache · world
  WorldTransform2D  map + drawing rotation/scale       node_component(capacity[, hook]) → ComponentDesc
  sin_cos, kQuarterTurn…, to_world, compose,           Hierarchy: a view of a world's node storage
  transform_point, inverse, relative, bounds             attach/detach/detach_children · queries · each_descendant
        ▲                                                  propagate() (the pass) · world_now · stats · debug_draw
        │ shared polynomial                                      │ entities-internal (src/entities/entities_internal.hpp):
  src/core/det_sine (sin_quarter, from spec 015)                 │   world_dying, world_busy, world_module_state,
        ▲                                                        │   world_reserve_many (all-or-nothing, N entities)
  src/tween/ease_math (unchanged bits)                 <nnh/prefabs.hpp>: PrefabOptions::children, nnh::child(),
                                                         spawn_under; src/prefabs/prefab_tree.cpp (the only prefab
<nnh/gfx.hpp>: gfx::transformed, gfx::draw(…, const      source including <nnh/hierarchy.hpp>, linked only through
  WorldTransform2D&, …) (src/gfx/draw_transform.cpp)     nnh::child())
```

### Decision D37: transforms as values with deterministic math; the hierarchy as one engine component kind in the game's world, computed by an explicit pass

**Transform values.** A `Transform2D` (local) is position, rotation (radians, clockwise on the y-down screen, the unit
of `DrawOptions::rotation`) and scale (a negative component mirrors). Its map is `p ↦ position + R(rotation)·S·p`
(scale, then rotate, then move). A `WorldTransform2D` holds the **affine map** (`Affine2D`: the images of the x and y
axes and the origin — Godot's `Transform2D`, so shear is representable and every point is exact) plus the **drawing
values**: an accumulated rotation and a product of scales (FR-2). Composition (`compose(parent_world, local)`):

```
s, c   = sin_cos(local.rotation)                          (cached per node, keyed by the rotation's bits)
Xr     = P.x · c + P.y · s          Yr = P.y · c − P.x · s    (P = the parent's map: its axes rotated)
map.x  = Xr · local.scale.x         map.y = Yr · local.scale.y
origin = (P.x · position.x + P.y · position.y) + P.origin      (component-wise, this order, no contraction)
mirror = signbit(P.scale.x) xor signbit(P.scale.y)          (integer bits: no float compare)
rotation (drawing) = P.rotation + (mirror ? −local.rotation : local.rotation)
scale (drawing)    = (P.scale.x · local.scale.x, P.scale.y · local.scale.y)
```

A **root** is not composed with an identity: `to_world(local)` builds `map.x = (c·sx, s·sx)`, `map.y = (−s·sy, c·sy)`,
`origin = position`, `rotation = local.rotation`, `scale = local.scale`, so a root's world equals its local bit for bit
(FR-3). A local with rotation `+0` and scale exactly `(1, 1)` (tested on the bits) composes as a **pure translation**:
the parent's axes and drawing values are copied and only the origin is computed (a fixed rule chosen by the local's
bits, so the pass, `world_now` and `compose` agree; it is also the platformer's and the bench's common case). The
drawing values are exact when the chain has no shear (every non-uniform scale sits on a node whose descendants are not
rotated relative to it, or the scale differs only in sign: a mirror — `diag(−1, 1)·R(θ) = R(−θ)·diag(−1, 1)`); with
shear they are Unity's `lossyScale` approximation while points stay exact.

**Deterministic sine.** `sin_cos(angle)`: `q = angle × float(2/π)` (one rounding), `n` = `q` rounded to the nearest
integer by adding ±0.5 (the sign taken from the bits) and truncating, `f = q − n` (exact), **`|f| < 2⁻²⁰` snaps to
0**, then by `n & 3` one of `±sin_quarter(|f|)` / `±sin_quarter(1 − |f|)` with the sign of `f` — spec 015's degree-7
polynomial (`sin_quarter`, exactly 0 at 0 and 1 at 1, error < 7·10⁻⁷), moved to `src/core/det_sine.{hpp,cpp}` with
its bits unchanged. So the engine's constants `kQuarterTurn` (`float(π/2)`), `kHalfTurn` (2×, exact),
`kThreeQuarterTurn` (3×, rounded once) and `kFullTurn` (4×) — and their small float multiples — give sine and cosine of
exactly 0 and ±1 (the snap's angle error, ≤ 2⁻²⁰ quarter turn ≈ 1.5·10⁻⁶ rad, is below the polynomial's own). Edge
cases have defined bits on every compiler: a non-finite angle gives `{0, 1}` (and asserts in Debug/Development);
`|q| ≥ 2²³` (q is then an integer) takes `n & 3` from the mantissa bits by an integer path, so no float→int
conversion ever overflows (x86 and ARM disagree on overflow).

**The hierarchy is one engine component kind in the game's world.** `nnh::Node` is a component like any other,
registered with `nnh::node_component(capacity[, hook])` (D35's storage, quarantine, scene binding and remove hooks
reused). It holds the game-writable `local` (a `Transform2D`: tweens target `&node->local.position` by pointer, a
prefab places `&Node::local`), the stored `world()` (read-only) and private links. **Links are node slots** (`u16`;
`0xFFFF` = none), not handles: following one is one address computation in the node array, never a record lookup.
Children keep attach order: parent, first child, next sibling, previous sibling, where the first child's previous is
the last child (O(1) append and unlink). A `Hierarchy` is a cheap view of a world's node storage (the kind lookup done
once, as `Storage<T>`), through which every operation goes.

**World transforms are computed by an explicit pass** (`Hierarchy::propagate()`): roots in ascending node-slot order,
each subtree depth-first in child order, each node once from its parent's fresh world and its own current local. It
walks only the node array, through parent links (no stack), and calls no game code. `world_now(e)` recomputes one
node's world from its ancestors' locals with the same inline code, bit for bit what a pass would store. A node's
**stored** world is computed when a `Node` is constructed from a local, when it is attached keep-local or detached
keep-local, when a prefab hierarchy is spawned, and by the pass; **keep-world reparenting leaves it unchanged** and
computes the new local from the stored worlds of the node and its new parent (the values last computed — what was
drawn; Bevy's `set_parent_in_place` does the same with its `GlobalTransform`), which needs no sine and no chain walk.

**Destroying a parent destroys its children through spec 020's queue.** The engine's remove hook for nodes (after the
game's, when given) unlinks the node from its parent; if the entity is being destroyed (its dying bit, read through an
entities-internal query) it detaches each child (the child's parent link cleared) and `destroy()`s it — queued, since
hooks are running — so a cascade is breadth-first in child order with no recursion, every descendant's hooks running
with its data intact and its parent already stale. If only the node component is removed, the children become roots
that keep their stored world (their local set from it). `clear()` destroys every node as spec 020 says; cascades merge
into it.

**Prefabs with children.** A prefab may list up to 8 child entries (a registered prefab of the same library and an
optional local); a spawn builds the whole tree all or nothing through one new entities-internal reservation of N
entities (`world_reserve_many`), creates parent first then children depth-first in entry order, links and computes
worlds, then runs the hooks children first. The tree code lives in one prefab source that the library reaches only
through a pointer set by `nnh::child()`, so a game whose prefabs have no children links none of it.

**Rejected:** a companion tree object beside a world (duplicate bookkeeping; still needs the remove hook); the game's
own struct read by member pointers (links can't be kept safe); position/rotation/scale-only worlds (points wrong under
shear); matrices only (drawing would need `atan2`/`sqrt`); dirty flags with lazy recomputation (locals would have to be
setters: tweens by pointer and prefab placement would bypass them); an automatic pass from scene binding (a new D30
notification, and it would run before the camera can follow the result); handles as links (a record lookup per hop:
two misses per node on the Old 3DS); recursion or an explicit stack in the pass or cascades; `std::sin` (different
bits per libm); lookup tables (memory, coarse angles); fresh (`world_now`) values for keep-world reparents (two chain
walks and a sine per reparent: ≈ 2× AC-7's reparent budget); a fused root-is-identity composition (a root's world
would lose its bits through `1·x + 0·y`).

### Public API (sketch)

```cpp
// <nnh/transform.hpp>: only <nnh/math.hpp>. Declarations only: every float operation is in src/transform/*.cpp,
// compiled with -ffp-contract=off (inline math here would be compiled with the game's flags).
namespace nnh {
inline constexpr float kQuarterTurn = 1.57079637f;          ///< float(π/2): sin_cos gives exactly (1, 0)
inline constexpr float kHalfTurn = 2.0f * kQuarterTurn;     ///< exact doubling: (0, −1)
inline constexpr float kThreeQuarterTurn = 3.0f * kQuarterTurn;
inline constexpr float kFullTurn = 4.0f * kQuarterTurn;

struct SinCos { float sin = 0; float cos = 1; };
/// The engine's sine and cosine (radians): the same bits on every system; exact 0/±1 at the constants above and their
/// small multiples; a non-finite angle gives {0, 1} (asserts in Debug/Development). No allocation, no libm.
SinCos sin_cos(float radians);

/// A local transform: scaled, then rotated (clockwise on screen), then moved. Default: the identity.
struct Transform2D { Vec2 position{}; float rotation = 0; Vec2 scale{1, 1}; };
/// An affine map: the images of the unit x and y axes, and of the origin (exact for points under any chain).
struct Affine2D { Vec2 x{1, 0}; Vec2 y{0, 1}; Vec2 origin{}; };
/// A world transform: the map (points, directions) and the drawing values (exact without shear).
struct WorldTransform2D {
    Affine2D map{};
    float rotation = 0;   ///< accumulated; a mirrored parent turns its children the other way
    Vec2 scale{1, 1};     ///< product of scales; a negative component = mirrored
    constexpr Vec2 position() const { return map.origin; }
};

WorldTransform2D to_world(const Transform2D& root);                                  ///< bit-identical drawing values
WorldTransform2D compose(const WorldTransform2D& parent, const Transform2D& local);
Vec2 transform_point(const Affine2D& map, Vec2 local_point);     ///< a muzzle, a box corner
Vec2 transform_direction(const Affine2D& map, Vec2 local_direction); ///< no translation
/// False (out untouched) when |determinant| < 2⁻¹⁰⁰ (a zero scale in the chain): an exponent test, the same on every system.
bool inverse(const Affine2D& map, Affine2D& out);
bool to_local(const Affine2D& map, Vec2 world_point, Vec2& out);
/// The local that puts a node at `world` under `parent` (keep-world reparenting): position exact within FR-3's bound,
/// rotation and scale from the drawing values (exact without shear). False when `parent` can't be inverted.
bool relative(const WorldTransform2D& parent, const WorldTransform2D& world, Transform2D& out);
/// The axis-aligned box of `local` under `map` (FR-14): unrotated maps give the mapped rectangle (x = origin.x +
/// a·r.x, w = |a|·r.w, mirrored rectangles normalised), rotated ones the box of the four mapped corners.
Rect bounds(const Affine2D& map, Rect local);
bool is_finite(const Transform2D& t); ///< integer test on the bits
} // namespace nnh

// <nnh/hierarchy.hpp>: <nnh/entities.hpp>, <nnh/transform.hpp>, nnh/detail/hierarchy_walk.hpp — not gfx, collision,
// camera, tween, animation, tilemap or prefabs.
namespace nnh {
enum class Screen : std::uint8_t;   // <nnh/gfx.hpp>: only debug_draw takes one

inline constexpr int kMaxHierarchyDepth = 16;   ///< levels: a root is depth 0, the deepest node depth 15

/// The engine's node component: register it with node_component(); reach it with world.get<nnh::Node>(e) or a
/// Hierarchy. 80 bytes on every system. Copying a Node copies its local and world, never its links.
class Node {
public:
    constexpr Node() = default;
    explicit Node(const Transform2D& local);  ///< its world is computed now (a root's)
    Node(const Node& other);                  ///< local, world, sine cache; links empty
    Node& operator=(const Node& other);       ///< the same; this node keeps its own links
    Transform2D local{};                      ///< yours to write (tweens by pointer, prefab placement)
    const WorldTransform2D& world() const { return world_; } ///< last computed: the pass, an attach, creation
    int depth() const { return depth_; }
private:
    friend struct detail::NodeAccess;
    // (declaration order = layout: links first; see Data and memory)
};

/// The node kind's registration (D35): `const nnh::ComponentDesc kinds[] = {nnh::node_component(256), ...}`.
ComponentDesc node_component(int capacity);
/// With the game's own remove hook (≤ 16 bytes of captures), run before the engine's (which unlinks or cascades).
template <typename F> ComponentDesc node_component(int capacity, F&& on_remove);

enum class Reparent : std::uint8_t { KeepWorld, KeepLocal };
/// Why an attach would be refused (can_attach: no assert), or Ok.
enum class AttachCheck : std::uint8_t { Ok, Stale, NoNode, Cycle, TooDeep, NotInvertible };

struct HierarchyStats {
    int nodes = 0, peak = 0, capacity = 0;  ///< the node kind's
    int roots = 0, deepest = 0, computed = 0; ///< as of the last pass
    int refusals = 0, bad_locals = 0;       ///< attaches refused, non-finite locals met by passes (since creation)
};

/// A view of a world's nodes (the kind lookup done once): cheap to copy, valid for the world's life. Empty (default,
/// or a world without the node kind: asserts) it answers nothing and its commands do nothing.
class Hierarchy {
public:
    Hierarchy() = default;
    explicit Hierarchy(World& world);

    // ---- Structure (FR-6, FR-10, FR-11) ----
    /// Appends `child` after `parent`'s children. KeepWorld (default): its stored world is unchanged, its local becomes
    /// relative() to the parent's stored world. KeepLocal: its local is kept, its world computed from the parent's.
    /// Attaching to the current parent changes nothing (true). Refused — false, an assert in Debug/Development, a
    /// stats refusal, nothing changed — for each AttachCheck reason. Linear in the moved subtree (its depths).
    bool attach(Entity child, Entity parent, Reparent mode = Reparent::KeepWorld);
    AttachCheck can_attach(Entity child, Entity parent, Reparent mode = Reparent::KeepWorld) const;
    bool detach(Entity child, Reparent mode = Reparent::KeepWorld);      ///< becomes a root
    int detach_children(Entity e, Reparent mode = Reparent::KeepWorld);  ///< returns how many

    // ---- Queries (FR-7) ----
    Entity parent(Entity e) const;           ///< constant time; null for a root, a stale handle or no node
    int depth(Entity e) const;               ///< constant time; −1 without a node
    Entity root(Entity e) const;
    int child_count(Entity e) const;         ///< constant time
    Entity first_child(Entity e) const;
    Entity next_sibling(Entity e) const;
    detail::ChildRange children(Entity e) const; ///< `for (nnh::Entity c : h.children(ship))`, attach order
    bool is_ancestor(Entity ancestor, Entity e) const;
    /// fn(Entity) for every descendant of `e` (not `e`), pre-order, children in attach order. Destroying, detaching,
    /// attaching and spawning inside are safe: what is destroyed is not visited, what is created is not visited.
    template <typename F> void each_descendant(Entity e, F&& fn);

    // ---- World transforms (FR-8, FR-9) ----
    /// The pass: every node's world, roots in ascending slots, each subtree depth-first. Once per frame, after moving
    /// things and before the camera and drawing. Inside an iteration of this world or a hook: asserts, does nothing.
    /// Returns the nodes computed. No allocation, no game code called.
    int propagate();
    WorldTransform2D world_now(Entity e) const;   ///< fresh, bit for bit what propagate() would store; identity if none
    const WorldTransform2D* world_transform(Entity e) const; ///< the stored one; null without a node
    Transform2D* local(Entity e);                 ///< null without a node
    Node* node(Entity e);

    HierarchyStats stats() const;
    /// Debug/Development: a line from each node to its parent and its local axes (8 px), on `screen`. Release: nothing.
    void debug_draw(Screen screen) const;
};
} // namespace nnh

// <nnh/gfx.hpp> (additions; `struct WorldTransform2D;` forward-declared, transform.hpp not included):
namespace nnh::gfx {
/// `given` with a world transform merged in (FR-13): rotation = world's + given's (the given one subtracted under a
/// mirrored world, as a child's), scale = world's × given's, everything else kept (region, pivot, flips, tint, layer).
DrawOptions transformed(const WorldTransform2D& world, const DrawOptions& given = {});
/// draw(screen, texture, world.position(), transformed(world, options)).
void draw(Screen screen, const Texture& texture, const WorldTransform2D& world, const DrawOptions& options = {});
}

// <nnh/prefabs.hpp> (additions; includes <nnh/transform.hpp>):
namespace nnh {
inline constexpr int kMaxPrefabChildren = 8;   ///< child entries per prefab (a variant's inherited ones count)
inline constexpr int kMaxPrefabTree = 32;      ///< entities one spawn makes
inline constexpr int kMaxPrefabTreeComponents = 128; ///< components one spawn makes
struct PrefabChild {                           ///< made by nnh::child()
    Prefab prefab{};
    Transform2D local{};
    bool has_local = false;                    ///< else the child prefab's own Node local
    const detail::PrefabTreeOps* tree = nullptr; ///< set by child(): links the tree code only for games that use it
};
PrefabChild child(Prefab prefab);                          // inline: { prefab, {}, false, &detail::kPrefabTreeOps }
PrefabChild child(Prefab prefab, const Transform2D& local);
// PrefabOptions gains: PrefabChild children[kMaxPrefabChildren] = {};  (the first null prefab ends the list)
// PrefabLibraryDesc gains: int max_children = 0;  (child entries over all prefabs; 0: spec 021's layout exactly)
// PrefabLibrary gains:
//   Entity spawn_under(World& world, Prefab prefab, Entity parent);          // root attached keep-local
//   Entity spawn_under(World& world, Prefab prefab, Entity parent, Vec2 at); // `at` is local to `parent`
//   int children(Prefab prefab, Prefab* out, int capacity) const;           // entry order; returns the total
// place(&Node::local) is accepted: a Transform2D member receives the position (its first member).
}
```

Exact names may still change while building; stage notes record any change. The only templates are `node_component`
with a hook (a lambda that calls the game's hook, then `detail::node_removed`; its capture is the game's, ≤ 16 bytes)
and `each_descendant` (a `detail::DescendantWalk` on the stack, an out-of-line `next()` per visit, `fn` inline).

### Events introduced

None (FR-20, constitution IV). The docs show a game publishing its own `ItemPickedUp{Entity item, Entity hand}`.

### Semantics fixed by the plan

**Attach** (`attach.cpp`). `can_attach` decides, in this order: `Stale` (child or parent null, stale, or dying — being
destroyed counts as stale here), `NoNode` (either lacks a node), `Cycle` (`child == parent` or `child` is an ancestor
of `parent`: a climb from `parent`, ≤ 15 steps), `TooDeep` (`depth(parent) + 1 + height(child's subtree) > 15`; the
height by a stackless walk of the subtree), `NotInvertible` (keep-world only: the parent's stored map fails
`inverse`). Then `attach`: attaching to the current parent returns true unchanged; otherwise unlink (from the old
parent, if any), append under the new parent, write the subtree's depths, then the transform: **KeepWorld** — `local =
relative(parent.world, node.world)`, stored world unchanged; **KeepLocal** — stored world = `compose(parent.world,
local)`. Descendants keep their locals and their stored worlds (they update at the next pass; `world_now` is always
fresh). `detach(KeepWorld)`: local = the stored world's origin, drawing rotation and scale (exact without shear; a
sheared world loses its shear at the next pass, documented); stored world unchanged. `detach(KeepLocal)`: stored world
= `to_world(local)`. A refusal counts in `stats().refusals` and asserts naming the reason (Debug/Development).

**The pass** (`propagate.cpp`). Refused (assert, return 0) inside an iteration of the world or a hook. For each live
node slot in ascending order whose parent is none: compute it with the root rule, then walk its subtree: `next =
first_child`, else climb through parents until a node with a next sibling (stopping at the root). Each node: a
finiteness test of its local (integer bits; Release: a non-finite local is treated as the identity for this pass and
counted in `bad_locals`), the sine cache (if the rotation's bits differ from the cached bits: `sin_cos`, cache
updated), the pure-translation rule or the full composition, the stores, `computed + 1`, the deepest depth. Stats
(roots, deepest, computed) are written once at the end.

**`world_now`** (`world_now.cpp`): climb to the root collecting node slots (≤ 16, a stack array of `u16`), then the
root rule and compositions downward with the same inline code, reading the cache when its bits match (the same bits as
computing). Returns the identity for a handle without a node.

**Cascades and removal** (`node_removed.cpp`, the engine hook, after the game's): unlink the node from its parent;
then if the entity is dying (`world_dying`): for each child in order — clear its parent and sibling links, then
`world.destroy(child)` (queued: hooks are running) — so grandchildren go when their parent's hook runs: breadth-first in
child order; otherwise (`remove<Node>`): each child becomes a root whose local is set from its stored world (as
`detach(KeepWorld)`), and its subtree's depths are rewritten. Either way the node's own links are cleared and active
walks are told (below). `parent(child)` inside a doomed child's hooks answers null.

**Walks** (`descendant_walk.cpp`). A `DescendantWalk` holds the path of handles from `e` (≤ 16 levels) and, for each
level, the next sibling to visit; it holds the world's iteration guard (nodes created during the walk are fresh:
skipped) and is linked into the world's module state while it runs. When a node is unlinked (destroy, detach, removal)
and it is some active walk's remembered next sibling, the walk's entry moves to the unlinked node's next sibling (D30's
cursor idea). After `fn`, the walk descends into the current node's first child if the node is still alive and still
linked under the path, else moves to the remembered sibling, climbing as needed. A node moved into the part not yet
walked is visited there; a node moved out is not.

**Children range**: it reads each child's next sibling before giving the child to you (destroying or detaching the
current child inside is safe; a later sibling destroyed ends the loop early, never unsafely; use `each_descendant` for
heavier changes).

**Misuse (FR-23)** — assert in Debug/Development, do nothing / answer nothing (or the identity) in Release: stale,
null or (when detectable) foreign handles; an entity without a node; cycles; too deep; keep-world under a
non-invertible parent; a non-finite local reaching the pass; the pass inside an iteration of the same world or from a
hook; a `Hierarchy` made from a world without the node kind; a prefab child without the node kind (registration:
`InvalidArgument`); a `PrefabChild` not made by `nnh::child()` (`InvalidArgument`). Handles carry no world id: a handle
of another world whose slot is valid here is not detectable (spec 020's FR-23).

### Entities-internal additions (no public change)

`engine/core/src/entities/entities_internal.hpp` gains (D35 note):

```cpp
namespace nnh::detail {
/// Per-world words for modules over worlds (the hierarchy: active walks, stats). Zeroed at creation; at the END of
/// WorldCold, so no existing field moves (World::spawn and world_add stay byte-identical).
struct WorldModuleState { void* walks = nullptr; std::uint32_t words[5] = {}; };
WorldModuleState& world_module_state(World& world);
bool world_dying(const World& world, Entity e);     ///< destroy() started for e (hooks running or queued)
bool world_busy(const World& world);                ///< inside an iteration or a remove hook
/// One entity of a group reservation: its kinds (indices into `kinds`) and tags.
struct ReserveEntity { int first_kind = 0; int kind_count = 0; const Tag* tags = nullptr; int tag_count = 0; };
/// All or nothing for `count` entities (spec 022 FR-17): exactly what `count` world_reserve() calls in a row would
/// take — the lowest free entity slots and, per kind, its lowest free slots, in list order — or nothing, counting the
/// refusal as the first failing world_reserve() would. `out[i]` = entity i, `components` = each reserved component's
/// address in list order. Room is found first (the k-th free slot by bit scans from the hint, no state change).
Entity world_reserve_many(World& world, const ReserveEntity* list, int count, const int* kinds, Entity* out,
                          void** components, ReserveRefusal& why, int* full_kind);
}
```

`world_reserve_many.cpp` (≈ 110 lines) uses `take_entity`/`attach_component` (the helpers `spawn()`, `world_add()` and
`world_reserve()` share). Proven against `count` × `world_reserve` on twin worlds (T2.2). Spec 020's `spawn()`
(109 instructions) and `world_add` (143) stay byte-identical (checked T2.4, T7.6).

### Prefab additions (D36 note)

- **Registration** (`library_add.cpp`, `prefab_flatten.cpp`, `prefab_tree_plan.cpp`): `options.children` up to the
  first null prefab; each must come from `nnh::child()` (its `tree` pointer, else `InvalidArgument`) and name a prefab
  of this library (registered earlier, so no cycle can exist); the parent and every child prefab must have the node kind
  among their entries (`tree->node_kind`, the type id of `Node`, defined in `prefab_tree.cpp`: no forward declaration
  of `Node` in the prefab header); variants inherit the base's children and append their own (≤ 8 in all,
  `LimitReached`); the tree's entity count ≤ 32, its component count ≤ 128 and its height ≤ 15 (`LimitReached`); the
  library's `max_children` capacity (`LimitReached`). Stored: the record's `spare` byte becomes `child_count`; when
  `max_children > 0` the block has a child-entry array (24 B each: prefab index, has-local, the local) and a per-prefab
  `{first child entry, tree entities, tree components, height}` (6 B); the library header keeps the `tree` ops pointer.
  With `max_children = 0` the block is spec 021's byte for byte (its layout tests unchanged).
- **Dispatch** (`prefab_spawn.cpp`): `if (record.child_count != 0) return b->tree->spawn(...)` before anything else —
  one load and a branch on a record already loaded (spec 021's spawn path otherwise unchanged; its bench re-run on the
  console, T10.2 run 4).
- **Tree spawn** (`prefab_tree.cpp`, the only prefab source that includes `<nnh/hierarchy.hpp>`): (1) flatten the tree
  into a stack list (≤ 32: prefab, parent index, local override) in creation order — parent first, children depth-first
  in entry order; (2) resolve each distinct prefab's kinds (`world_kinds_of`), append the root's override-only kinds;
  (3) check the depth under the spawn's parent (`spawn_under`: `depth(parent) + 1 + height ≤ 15`; refused as a full
  capacity: a library miss, `WhenFull`'s assert or quiet miss); (4) `world_reserve_many` — refused: as spec 021's FR-8;
  (5) per entity in creation order: destroy each slot's `T{}`, copy the stored value (the root: or its override), the
  root's placement, (6) the root's node world `to_world(local)` (after placement), then each child in creation order:
  its entry's local if any, linked last under its parent, depth, world `compose(parent.world, local)` — exactly
  `attach(child, parent, KeepLocal)`; `spawn_under` attaches the root keep-local under `parent` the same way; (7)
  spawns + 1; (8) hooks in post-order (children before parents; each entity's chain base first), every `SpawnInfo`
  carrying the root's map object; (9) return the root's handle (stale if a hook destroyed it).
- **Equivalence** (FR-17, AC-2's hand side): for each entity in creation order `spawn()`, `add<T>(e, value)` per entry
  (the node as `add<Node>(e, Node(local_with_position_and_entry_local))`, whose world equals the tree spawn's), the
  placement member write, `add_tag` per tag, `attach(e, parent, KeepLocal)`; then the hooks in post-order.
- **Placement through a node**: `place(&Node::local)` (`M = Transform2D`, `static_assert(offsetof(Transform2D,
  position) == 0)`) stores a `Vec2` placement at that member's offset. A single-entity prefab placed this way gets the
  position in its local; its stored world is the stored value's until the next pass (spec 021's spawn path is
  unchanged: documented). Tree spawns recompute the root's world after placement (step 6).
- **Map objects**: `spawn_map_objects` goes through `prefab_spawn`, which dispatches trees; each child's `SpawnInfo`
  has the root's object and index and no position.

### Header and build cost (FR-24, III)

- `<nnh/transform.hpp>`: `<nnh/math.hpp>` only; ≈ 110 lines; no templates.
- `<nnh/hierarchy.hpp>`: `<nnh/entities.hpp>`, `<nnh/transform.hpp>`, `nnh/detail/hierarchy_walk.hpp` (`ChildRange`,
  `DescendantWalk`, `NodeAccess` declaration); ≈ 220 lines with comments. Budget: `-fsyntax-only` of a one-line TU
  within 0.05 s of `<nnh/entities.hpp>`'s (T0.1).
- `gfx.hpp` and `prefabs.hpp`: declarations and one include of the transform header (prefabs); `hello` doesn't include
  prefabs.
- Opt-in by linking: the transform functions live in their own objects; the hierarchy's in its own; the gfx overload
  in `draw_transform.o`; the prefab tree in `prefab_tree.o`, referenced only by `detail::kPrefabTreeOps`, which only
  the inline `nnh::child()` names. Checked with probes in T8.3 (transform-only game: no `nnh::World`; prefabs without
  children: no hierarchy or `prefab_tree` symbol; `hello`: sizes and `nm` equal T0.1).
- Every new float source (`_nnh_transform_sources` incl. `core/det_sine.cpp`, `_nnh_hierarchy_sources`,
  `gfx/draw_transform.cpp`, `prefabs/prefab_tree.cpp`) gets `-ffp-contract=off` in `engine/core/CMakeLists.txt` (the
  property is per source list there, not global).

### ARM11 cost plan (AC-7)

Lessons from specs 019–021 on the Old 3DS (ARM11 268 MHz, 16 KB L1D with 32-byte lines, no L2, no hardware
prefetcher, ≈ 110 cycles per miss), built in:

1. **Count misses per node, not only instructions.** Calibration from spec 020's console runs: a sequential walk
   without `pld` costs ≈ 80–110 cycles per line (`each` 0.475 ms for 1024 entities of 27 instructions); with a `pld`
   a few records ahead ≈ 40 cycles per line (`find`, 0.144 ms); random access ≈ 110 per line. Spawn-like paths ran at
   ≈ 2.1–2.3 cycles per instruction misses included (spec 021). The estimates below use these, then multiply the upper
   end by 1.3 (the instruction counts of specs 019–021 were 1.3–2× low).
2. **An 80-byte record ordered by use** (links, depth, local, sine cache, world): the pass touches ≈ 2.5 lines per node,
   `world_now`'s climb and compose mostly the first 44 bytes.
3. **The pass walks only the node array** (no entity records, no handle lookups: links are slots), depth-first through
   parent links (no stack), and **prefetches two nodes ahead** (`__builtin_prefetch` on integer addresses, two `pld` per
   node; a prefab-built hierarchy occupies consecutive slots in visiting order, so the walk is sequential).
4. **No out-of-line call per node**: composition and the sine are inline (`src/transform/compose.hpp`,
   `sin_cos.hpp`, `always_inline`) in `propagate.cpp` and `world_now.cpp`; the sine runs only when the rotation's bits
   changed; a pure-translation local skips 14 of the composition's float operations.
5. **No `vmrs`**: signs, mirror and finiteness are integer tests on float bits; the inverse's threshold is an exponent
   test; `bounds` (not an AC-7 path) may use float compares. `sin_quarter`'s clamp `v < 1 ? v : 1` compiles to a
   `vcmp`/`vmrs` pair today; it runs only when a rotation changed. If T7.6 shows it matters, it becomes a signed
   integer compare of the bits (identical for every non-NaN input, which is all `sin_cos` and the easings pass;
   proven over every float in [0, 1] by a one-off tool and a sampled unit test) — decided at T7.6, not before.
6. **No `__aeabi_*`** (no integer division or modulo: quadrants `& 3`, bench indices by masks), **no `memcpy`** (the
   `Node` copy constructor copies members; a 36-byte `WorldTransform2D` is `vldm`/`vstm`), **no `strcmp`**.
7. **Constant multiplies**: the node stride (80) is two shift-adds; T7.6 checks GCC made no long shift-add chain in
   a hot loop (spec 021's FNV lesson).
8. **FPSCR**: the 3DS runs round-to-nearest with flush-to-zero; the self-check's values stay normal; the PC doesn't flush.
9. **Keep-world reparents use stored worlds** (no sine, no chain walk: three `vdiv` and ≈ 200 instructions); the
   reparent bench's working set stays under 16 KB, as a game's handful of held items does.

Estimates (per node or call, then the workload; 1 ms = 268 000 cycles):

| AC-7 workload | Hot path | Estimate (upper × 1.3) | Target | Verdict |
|---------------|----------|------------------------|--------|---------|
| pass, 1024 nodes (256 × root, 2 children, a grandchild), every root moved and turned, one child turned | ≈ 80 instructions per node (bitmap walk and root test 10, DFS step 10, finiteness 8, cache test 4, composition 27 VFP + 7 loads/stores, sign 3, prefetch 2, stats 3) ≈ 200 cycles with 2.5 lines prefetched; 512 sines ≈ 110 cycles each | 0.85–1.25 ms (1.6) | < 1.5 ms | expected PASS; at risk at the upper end |
| the same pass, positions only (no rotation or scale changed) | no sine; 512 nodes on the pure-translation rule (≈ 60 instructions) | 0.65–1.0 ms (1.3) | < 1.0 ms | **at risk** (risk 1) |
| 256 `world_now` of depth-3 nodes in shuffled hierarchies | 4 dependent misses (≈ 440 cycles, the second line of each record prefetched during the climb) + ≈ 270 instructions | 0.95–1.1 ms (1.4) | < 1.5 ms | expected PASS, tight |
| 64 prefab hierarchies of 4 (node + 2 kinds, 1 tag) spawned, root destroys (cascades), one flush; the same by hand | prefab ≈ 320 instructions per entity (group reservation, copies, links, 3 compositions per tree) + destroy ≈ 445 (spec 021's 350 + the node hook ≈ 95) ≈ 765 × 2.23 cycles; hand ≈ 1030 per entity | 1.6–1.75 ms (2.1); hand ≈ 2.2–2.4; ratio ≈ 0.75 | < 2.2 ms, ≤ 1.3× | expected PASS, tight |
| 256 keep-world + 256 keep-local reparents between rotated and scaled parents (32 parents, 64 items) | keep-world ≈ 200 instructions + 3 `vdiv` ≈ 460 cycles; keep-local ≈ 175 + a sine (the item's rotation changed by the keep-world before) ≈ 450 | 0.8–0.95 ms (1.25) | < 1.0 ms | **at risk** (risk 1) |

As AC-7 says, the targets stay as written; T7.6 checks the disassembly against this table before the console run, the
console measures (T10.2), a missed target is optimised first (risk 1's list), then the budget is set from the
measurement with ≈ 25 % margin and written in the spec and the docs with the numbers.

### Self-check and bench (AC-2, AC-6, AC-7)

- **Self-check** (`examples/hierarchy/self_check*`, pure). Kinds: `Node` (engine), `A` (`Rect box`, `u32 layers`: 20
  B), `B` (`u32[4]`), `C` (`u16, u16`, a user-provided destructor), `D` (`u32`; its remove hook sometimes attaches an
  unrelated node or destroys another entity — "a hook that destroys or attaches during a cascade"). Two worlds of 1024
  entities, 4 tags, node capacity 768, `A` 512, `B` 300 (full kinds happen). One library (`WhenFull::Miss`,
  `max_children` 48) of **12 prefabs**: a bare node, node + A, `ship` (gun → muzzle; ship has B), `gun`, `muzzle`, a
  three-level `tower`, an 8-child `ring`, a 32-entity `swarm` (ring of rings), a variant of `ship` appending a `shield`
  child, `shield`, a prefab without a node (plain spawn), a node prefab with a hook that spawns another prefab under
  itself. The **hand side** never reads the library: its 12 definitions are written out flattened by hand (creation
  order, values, tags, placement, entry locals, hook functions), and it checks room first (entities, each kind: live +
  quarantined counts kept by the side, as spec 021's) and depth, then calls `spawn`/`add`/`add_tag`/`attach(KeepLocal)`
  and the hooks in post-order. **50 000 operations** chosen by FNV-1a of (seed, step, salt), no `rand`: spawns (root,
  at a position, under a parent; by prefab on one side and by hand on the other), local writes from seeded integers
  (angles `k·kQuarterTurn` and `k/1024` rad, positions `k/4`, scales from {1, −1, ½, 2, −¾, (2, ½), (−1, 1), 0}),
  attaches and detaches in both modes (each preceded by `can_attach`; a refusal is a counted outcome, never an
  assert), `detach_children`, destroys (cascading; also inside `each_descendant`), `remove<Node>`, flushes, rarely
  `clear()`, and a **pass every 64 operations**, after which every node's `world_now` must equal its stored world (bits)
  on both sides. After every step both sides returned the same handle bits; every 1000 steps and after each clear a
  full-state FNV-1a of both worlds (live handles, each kind's live slots, owners and bytes, tags, and per node: parent,
  depth, child order, the world's 9 floats' bits) must be equal; the running checksum gives a checkpoint every 10 000
  operations. Goldens recorded from pc-debug in `kHierarchyCheckGolden`, equal in pc-release; the unit test and ctest
  `hierarchy_self_check_pc` hold them; the console logs `hierarchy: <n> sum <checksum>` and `hierarchy: PASS
  <checksum>` / `hierarchy: FAIL step <n> ...`, 2000 operations per frame.
- **Bench** (`examples/hierarchy/bench.{hpp,cpp}` pure workloads, `bench_scene` timing them with the profiler, 300
  frames each, average and worst; only the measured call inside the profile scope): `pass_turn` and `pass_move` (one
  world of 1024 nodes built by prefab in visiting order; the moves outside the scope), `world_now` (the same shape built
  by hand in a seeded shuffled order, so records scatter; 256 queries of the depth-3 nodes in a shuffled order),
  `spawn` (two worlds of 256 entities, the same description, prefab and hand on alternate frames: one ratio per run),
  `reparent` (16 rotated, scaled roots with a child each = 32 parents, 64 items; 256 keep-world then 256 keep-local per
  frame, indices by masks). One line per workload: `hierarchy bench: <name> avg X ms worst Y ms (target < Z ms)
  PASS|FAIL` (the spawn line adds `hand H ms ratio R (<= 1.3)`), then `hierarchy bench: PASS` or `hierarchy bench: FAIL
  (<names>)`; a Debug build logs `not judged (Debug)`.
- **The script** `examples/hierarchy/tests/check.nnhs`: SELECT → every checkpoint and the PASS line; the showcase's
  fixed inputs and touches with expected lines (below); screenshots; Y → the bench lines and the verdict.
  `fail-on "hierarchy: FAIL"`, `fail-on "hierarchy bench: FAIL"`, `fail-on "] E "`. A `wait 3` follows every `expect`
  that an input follows (spec 021's console lesson).

### The example `examples/hierarchy` (AC-5)

A yard 640 × 240 world pixels. **Top screen**: a `CameraRig` follows the cart's rider. **Bottom screen**: the whole
yard at zoom 0.5 (a fixed `Camera2D`) and the HUD; touches pick there. Things (original art from `art/make_art.py`):
the **ship** (Circle Pad moves, L/R turn) with a **gun** (D-Pad left/right aims; A fires bullets from its **muzzle**
point: `transform_point(world_now(muzzle).map, {0, 0})`) and a **hand** node where items hang; a **walker** walking
between two posts and mirrored with `scale.x = −1` when it turns, its **hat** bobbing (a tween on `&hat->local.position`);
the **cart** with a **rider**; a **squash** parent with scale (2, ½) and a rotated child (shear: its muzzle point is
exact, its sprite drawn with the approximation); two **crates** lying on the ground (items). Order per frame: input →
moves and tweens → `propagate()` → bodies from bounds (`each<Node, Body>`: `bounds(world.map, local_box)`) → camera rig
→ draw (each node through `gfx::draw(screen, tex, world, options)`). Controls: a touch picks the node under it
(`collision::boxes_at` on the body view) and logs `picked: <name> slot <s> gen <g> depth <d> at <x>,<y>`; X attaches the
picked item to the ship's hand (keep world; logs `attached: <name> to hand at <x>,<y>`) or, when held, drops it
(`dropped: <name> at <x>,<y>` — the same stored position); B destroys the ship (`destroyed: ship + <n> children`, the
HUD count drops); D-Pad up detaches the ship's children first (`detached: <n> children`); D-Pad down toggles the debug
draw; SELECT runs the self-check, Y the bench, START quits. Positions are binary fractions (prints are exact). Log
lines in frame 0 are re-logged by a restart (X with nothing picked: `yard: restarted nodes <n>`), as spec 021's map
line.

### The platformer (AC-4, decision 24)

**What changes.** `level::create_world` registers two more kinds at the end (`nnh::node_component(8)` and
`level::Barrel{}` (8): a frame index and a flag), so the existing kinds keep their indices and lookups. The library
gains `barrel` (Node + Barrel, tag `barrel`) and the `turret` prefab gains the Node kind (its stored
`Node()`), the child entry `child(barrel)` (local at the turret's centre-relative origin, `(0, 0)`), and a hook that
sets the turret node's `local.position` to the body box's centre (placement stays `&Body::box`: the collision box's
bits don't change). `max_prefabs` 4, `max_entries` 9, `value_bytes` 256, `max_children` 1. `TurretBook` takes a
`Hierarchy`; in `look()`, while a turret sees the player its barrel's `local.rotation` is set to the direction of
`aim(d)` (one of 8 angles, `k × kQuarterTurn / 2`: a table), and a shot starts at
`transform_point(world_now(barrel).map, kMuzzle)` (`kMuzzle = {10, 0}`) instead of the box centre (the velocity is
still `aim(d)`). `LevelPlay::update()` calls `hierarchy_.propagate()` after `turrets_.update()` and before the camera;
`state()` additionally hashes each barrel's stored world origin and rotation bits (turret order). `draw_turrets()`
draws the base and the barrel from a new original image `turret.png` (base 16 × 16, barrel 12 × 4, pivot at the
barrel's back) through `gfx::draw(kTop, tex, world, options)`. The HUD's entity count includes the barrels.

**Why every replay line changes, and how they are regenerated.** Each turret spawns a barrel entity right after it
(slots shift: every later coin and sparkle takes the next slot), so every sample's live count (+2 in level 1) and slot
hash change; bullets start 10 px along the barrel (the engine's sine under the replay), so the turret state changes;
the state hashes the barrels' world bits. The x, y and coins columns must **not** change (bullets never move the
player; no coin or body moved): if they do, the stage stops and this plan says why. Procedure (T6.3), in the commit
that adds the barrels: (1) ctest `platformer_replay_matches_script` fails as expected; (2) `PLATFORMER_DEMO=replay
NNH_EXIT_AFTER_FRAMES=920 build/pc-debug/examples/platformer/platformer.exe`, copy its 15 `replay: … sum` lines into
`examples/platformer/tests/level.nnhs` (the last up to its colon); (3) the same run in pc-release prints identical lines;
(4) the header comment says "Last regenerated by spec 022 T6.3: each turret has a barrel child entity aimed in the 8
directions; bullets leave from its muzzle point; the turret state hashes the barrels' world transforms; every sum
changed, the x, y and coins columns did not"; (5) the ctest passes and `nnh run platformer --pc --no-build --script
examples/platformer/tests/level.nnhs` → PASS; (6) stage 9 checks the ctest on GCC 13/14 and clang 18; (7) the
console run (T10.2 run 2) must print exactly these lines with `60 Hz PASS`. The benchmark, zoom and flip demos are not
touched (their scenes don't spawn turrets).

### Files

| File | New/changed | Responsibility |
|------|-------------|----------------|
| `engine/core/include/nnh/transform.hpp` | new | the transform values and math (declarations) |
| `engine/core/src/core/det_sine.{hpp,cpp}` | new | `sin_quarter` moved from `tween/ease_math.cpp` (bits unchanged) |
| `engine/core/src/tween/ease_math.{hpp,cpp}` | changed | include `core/det_sine.hpp`; `sin_quarter` removed from the `.cpp` |
| `engine/core/src/transform/sin_cos.{hpp,cpp}` | new | inline reduction, snap, quadrants, edge cases; public `sin_cos` |
| `engine/core/src/transform/compose.{hpp,cpp}` | new | inline root rule, translation rule, composition, drawing values; public `to_world`, `compose` |
| `engine/core/src/transform/affine.cpp` | new | `transform_point`, `transform_direction`, `inverse`, `to_local`, `is_finite` |
| `engine/core/src/transform/relative.cpp` | new | keep-world math |
| `engine/core/src/transform/bounds.cpp` | new | axis-aligned bounds |
| `engine/core/include/nnh/hierarchy.hpp` | new | `Node`, `node_component`, `Reparent`, `AttachCheck`, `HierarchyStats`, `Hierarchy` |
| `engine/core/include/nnh/detail/hierarchy_walk.hpp` | new | `ChildRange`, `DescendantWalk`, `NodeAccess` declaration |
| `engine/core/src/hierarchy/hierarchy_internal.hpp` | new | `NodeAccess`, inline link helpers, the module-state words, log channel `hierarchy` |
| `engine/core/src/hierarchy/node.cpp` | new | `Node(local)`, copy and assignment, `node_component`, the view's constructor |
| `engine/core/src/hierarchy/node_links.cpp` | new | link, unlink (telling walks), subtree height and depths (stackless) |
| `engine/core/src/hierarchy/attach.cpp` | new | `can_attach`, `attach`, `detach`, `detach_children`, refusals |
| `engine/core/src/hierarchy/hierarchy_queries.cpp` | new | `parent`, `depth`, `root`, `child_count`, `first_child`, `next_sibling`, `is_ancestor`, `local`, `node`, `world_transform`, `stats` |
| `engine/core/src/hierarchy/propagate.cpp` | new | the pass |
| `engine/core/src/hierarchy/world_now.cpp` | new | `world_now` |
| `engine/core/src/hierarchy/descendant_walk.cpp` | new | the walk and its cursor fix-ups |
| `engine/core/src/hierarchy/node_removed.cpp` | new | the engine's remove hook: cascade or removal |
| `engine/core/src/hierarchy/hierarchy_debug_draw.cpp` | new | debug draw (Debug/Development), the only hierarchy source including gfx |
| `engine/core/src/entities/entities_internal.hpp`, `world_internal.hpp` | changed | the additions above; `WorldCold` gains `WorldModuleState` at its end |
| `engine/core/src/entities/world_reserve_many.cpp`, `world_module.cpp` | new | group reservation; `world_module_state`, `world_dying`, `world_busy` |
| `engine/core/include/nnh/gfx.hpp`, `engine/core/src/gfx/draw_transform.cpp` | changed / new | `gfx::transformed`, the `draw` overload |
| `engine/core/include/nnh/prefabs.hpp`, `detail/prefab_values.hpp` | changed | `PrefabChild`, `child()`, limits, `children`, `max_children`, `spawn_under`, `children()` query, `place` of a `Transform2D` |
| `engine/core/src/prefabs/prefab_internal.hpp`, `library_layout.{hpp,cpp}`, `library_add.cpp`, `prefab_flatten.cpp`, `library_queries.cpp`, `prefab_spawn.cpp` | changed | child storage, checks, inherited children, the dispatch |
| `engine/core/src/prefabs/prefab_tree_plan.cpp`, `prefab_tree.cpp` | new | tree facts at registration; the tree spawn and `spawn_under` (the only prefab source including `<nnh/hierarchy.hpp>`) |
| `engine/core/CMakeLists.txt` | changed | `_nnh_transform_sources`, `_nnh_hierarchy_sources`, new files, `-ffp-contract=off` on each float list |
| `tests/transform/test_sin_cos.cpp`, `test_compose.cpp`, `test_affine.cpp`, `test_relative.cpp`, `test_bounds.cpp`, `test_accuracy.cpp` | new | FR-1–FR-4, AC-3 |
| `tests/entities/test_world_reserve_many.cpp`, `test_world_module.cpp` | new | the entities-internal additions |
| `tests/hierarchy/hierarchy_fixture.hpp`, `test_node.cpp`, `test_attach.cpp`, `test_keep_world.cpp`, `test_queries.cpp`, `test_propagate.cpp`, `test_world_now.cpp`, `test_each_descendant.cpp`, `test_cascade.cpp`, `test_remove_node.cpp`, `test_scene_binding.cpp`, `test_camera_follow.cpp`, `test_debug_draw.cpp`, `test_hierarchy_misuse.cpp` | new | FR-5–FR-15, FR-22, FR-23 |
| `tests/gfx/test_draw_transform.cpp` | new | FR-13 on the fake platform's recorded items |
| `tests/prefabs/test_children.cpp`, `test_tree_spawn.cpp`, `test_tree_all_or_nothing.cpp`, `test_spawn_under.cpp`, `test_map_trees.cpp` | new | FR-16–FR-18 |
| `tests/data/maps/make_fixtures.py`, `good_prefab_trees.tmj` | changed / new | a map whose objects name tree prefabs (spec 021's fixture untouched) |
| `tests/hierarchy_example/test_self_check.cpp` | new | AC-2 golden |
| `tests/fuzz/fuzz_hierarchy_ops.cpp`, `fuzz_transform.cpp` | new | bytes → self-check operations; random float bits → transform functions (no UB, finite where promised) |
| `tests/CMakeLists.txt` | changed | the tests, the fixture conversion, ctests `hierarchy_self_check_pc`, `hierarchy_check_script_pc` |
| `examples/hierarchy/` | new | `main.cpp`, `demos.hpp` (`HIERARCHY_DEMO`), `components.hpp`, `yard.{hpp,cpp}` (world, library, hierarchy, the frame order), `yard_prefabs.cpp`, `ship.{hpp,cpp}`, `walker.{hpp,cpp}`, `cart.{hpp,cpp}`, `picking.{hpp,cpp}`, `yard_draw.{hpp,cpp}`, `hud.{hpp,cpp}`, `showcase_scene.{hpp,cpp}`, `check_kinds.hpp`, `self_check_defs.cpp`, `self_check_sides.{hpp,cpp}`, `self_check.{hpp,cpp}`, `self_check_scene.{hpp,cpp}`, `bench.{hpp,cpp}`, `bench_scene.{hpp,cpp}`, `art/make_art.py`, `assets/art/*.png`, `CMakeLists.txt`, `tests/check.nnhs` |
| `examples/CMakeLists.txt` | changed | `add_subdirectory(hierarchy)` |
| `examples/platformer/level_world.{hpp,cpp}`, `level_prefabs.{hpp,cpp}`, `turrets.{hpp,cpp}`, `level_play.{hpp,cpp}`, `art/make_turret.py`, `assets/art/turret.png`, `CMakeLists.txt`, `tests/level.nnhs` | changed / new | barrels (above); the replay lines regenerated |
| `tests/platformer/test_barrels.cpp`, `test_turrets.cpp`, `test_level_prefabs.cpp` | new / changed | barrels aim in 8 directions, muzzle shots, a turret with its barrel from a map object |
| docs (stage 11) | new / changed | `docs/api/transform.md`, `docs/api/hierarchy.md` (new); `math.md`, `entities.md`, `prefabs.md`, `gfx.md`, `camera.md`, `guides/levels.md`, `architecture/decisions.md` (D37 implemented), `patterns.md`, `scenes.md`, `docs/REFERENCES.md`, `CLAUDE.md`, `docs/ROADMAP.md`; `scripts/check_release_strings.sh` if the module gets a Development-only text |

### Data and memory

| Item | Size (3DS / 64-bit PC) | Notes |
|------|------------------------|-------|
| `Transform2D` | 20 B | position, rotation, scale |
| `Affine2D` | 24 B | |
| `WorldTransform2D` | 36 B | map + drawing rotation + drawing scale |
| `Node` | **80 B** / 80 B | parent, first child, next, previous (`u16` × 4) 8 · depth `u8`, flags `u8`, child count `u16` 4 · local 20 · sine cache (rotation bits, sin, cos) 12 · world 36; `static_assert`ed |
| per node slot | 80 + 2.5 B | spec 020's kind overhead (bitmaps, owner) |
| per world | + 24 B / 28 B | `WorldModuleState` in the cold part (every world, used or not) |
| `Hierarchy` | ≈ 36 B / 56 B | the view (a `Storage<Node>` and the world's header) |
| `DescendantWalk` | ≈ 120 B (stack) | 17 handles + 17 next slots + links |
| `world_now` | 32 B (stack) | 16 `u16` slots |
| the pass, cascades | 0 | parent links; spec 020's queue |
| prefab child entry | 24 B | when `max_children > 0`; + 6 B per prefab for the tree facts |
| tree spawn | ≈ 2.3 KB / 3.3 KB (stack) | the creation list (32 × 28 B), kinds and addresses (128 × 4/8 B), per-entity reservations |

1024 nodes ≈ 82.5 KB in the world's block (the spec expected ≤ 96 KB). The sine cache (open question 3) is kept: 12 B
per node against ≈ 110 cycles per node per frame for unchanged rotations (the platformer's and the bench's common
case); T10.2 measures `pass_move`, which would show it. Everything is CPU data in the Heap region.

### Platform work

- **pc / 3ds:** none. Portable core code, no contract change; the 3DS runs the self-check, the showcase, the bench
  and the platformer (stage 10).
- **fake:** none. Tests run on the fake platform; the drawing helper's tests read the fake's recorded 2D items.

## Test strategy

All unit tests run in `nnh_tests` on the fake platform (Windows and VM 120, GCC 13/14 and clang 18, ASan/UBSan). A
`double` reference composes the same chains with the engine's sine values converted to double (composition rounding:
FR-3's 1/32 px) and, separately, with `std::sin`/`std::cos` (the whole error, documented). The assert hook catches
misuse; the allocation counter checks "no allocation after creation"; spec 020's twin-world pattern checks the group
reservation and the prefab trees (prefab side vs hand side), and AC-2's script does it over 50 000 operations.

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | `test_compose.cpp`: identity default; scale → rotate → move order on a point; root rule bit-equal to the local |
| FR-2 | `test_compose.cpp`, `test_affine.cpp`: composition, points, directions, inverse round trips, `to_local`; shear chains: points exact against the reference, drawing values as documented (accumulated rotation, product of scales) |
| FR-3 | `test_sin_cos.cpp`: quarter-turn constants and their multiples −8..8 exact, symmetry (`sin(−x) = −sin(x)` bits), a 4096-angle reference table (error < 7·10⁻⁷ + snap), non-finite → {0, 1} + assert, `|q| ≥ 2²³` integer path; `sin_quarter` and the easings unchanged (spec 015's tests, `tweens: PASS B44CA3F6`); `test_accuracy.cpp` (AC-3); AC-2 on every compiler and the console |
| FR-4 | `test_compose.cpp`: `scale.x = −1` on a root mirrors children's positions, turns their rotation the other way, carries the flip in the drawing scale; flips not inherited (`test_draw_transform.cpp`) |
| FR-5 | `test_node.cpp`: `sizeof(Node) == 80`, defaults, `Node(local)` computes a root world, copy keeps no links, assignment keeps links, `node_component` (with and without a game hook: game hook first), worlds without the kind untouched |
| FR-6 | `test_attach.cpp`: append order, detach, attach to the current parent, each refusal (stale, dying, no node, cycle, self, depth 16, keep-world under a zero scale) leaves everything byte-identical (block hash) and asserts; `can_attach` answers the same without asserting |
| FR-7 | `test_queries.cpp`: `parent`, `depth`, `root`, `child_count`, children range and first/next walk, `is_ancestor`; `test_each_descendant.cpp`: pre-order, destroy (current, ancestor, next sibling, unvisited subtree), detach, attach and spawn inside |
| FR-8 | `test_propagate.cpp`: order (a call log of computed slots), each node once, roots bit-equal to locals, a node created or attached between passes holds its at-creation world, rotation-unchanged nodes skip the sine (a counter in test builds), refused inside `each` and hooks |
| FR-9 | `test_world_now.cpp`: equals the pass's stored bits for every node of seeded hierarchies, before and after moves; the muzzle pattern |
| FR-10 | `test_keep_world.cpp`: stored world unchanged by keep-world attach/detach; the local's world after a pass within the bound; exact for no-shear chains with power-of-two scales and quarter turns; descendants keep locals |
| FR-11 | `test_cascade.cpp`: every descendant destroyed, hooks breadth-first in child order with node data and last world intact, parent stale and `parent(child)` null in a child's hook, hooks that destroy or attach during a cascade, `detach_children` first, `clear()` with hierarchies and the first load's slots after it; `test_remove_node.cpp`: children become roots keeping their stored worlds, depths rewritten |
| FR-12 | `test_scene_binding.cpp`: a bound world with nodes flushed and cleared with its scene; node slots quarantined until the flush |
| FR-13 | `test_draw_transform.cpp`: the fake's recorded item (position, rotation, scale, flips) for given and animator options, mirrored worlds; camera zoom applies as any draw |
| FR-14 | `test_bounds.cpp`: unrotated (exact, `w = |a|·r.w`), mirrored (normalised), rotated (enclosing), quarter turns exact; the `each<Node, Body>` pattern read by `boxes_at` |
| FR-15 | `test_camera_follow.cpp`: a `CameraRig` following a child's stored world after the pass equals one following the hand-computed point |
| FR-16 | `test_children.cpp`: registration limits (9 entries, 33 entities, 129 components, depth 16, `max_children`), missing node kind, a child of another library, a `PrefabChild` without `child()`, variants appending children, the block hash unchanged on refusal, spec 021's layout with `max_children = 0` |
| FR-17 | `test_tree_spawn.cpp`: creation order, links, worlds, post-order hooks, twin worlds equal (handles, slots, bytes, tags, links, world bits); `test_tree_all_or_nothing.cpp`: full world, each kind full, too deep under a parent — nothing created, misses counted, `WhenFull` |
| FR-18 | `test_spawn_under.cpp`: under a parent, position local, depth; `test_map_trees.cpp`: `good_prefab_trees.nnhmap`: whole hierarchies from objects, the children's `SpawnInfo` object |
| FR-19 | AC-2's golden on WinLibs GCC, GCC 13/14, clang 18 and the console |
| FR-20 | `test_attach.cpp`: `events::stats()` unchanged by every operation |
| FR-21 | `test_hierarchy_misuse.cpp`: allocation counter unchanged over 100 000 operations after creation, no log line in the normal path; AC-7 |
| FR-22 | `test_queries.cpp` (stats), `test_debug_draw.cpp` (the fake's recorded lines in Debug; none in the Release path) |
| FR-23 | `test_hierarchy_misuse.cpp`: every listed misuse asserts and is harmless in Release (pc-release run) |
| FR-24 | include check (`hierarchy.hpp` and `transform.hpp` pull no gfx/collision/camera/tween/animation/tilemap/prefab header); T8.3 probes; spec 020/021 tests and goldens unchanged |
| AC-2 | `tests/hierarchy_example/test_self_check.cpp`, ctest `hierarchy_self_check_pc`, the console's `hierarchy: PASS` |
| AC-3 | `test_accuracy.cpp`: seeded hierarchies (depth ≤ 16, coordinates within ±4096 px, scales ¼–4) vs the reference; exactness cases |
| AC-4 | `test_barrels.cpp`, ctest `platformer_replay_matches_script` (new lines), `level.nnhs --pc`, a screenshot read back |
| AC-5 | PC screenshots (showcase, mirrored walker, pick-up and drop, cascade, debug draw) read back |
| AC-6 | `examples/hierarchy/tests/check.nnhs` with `--pc`, then on the console |
| AC-7 | `hierarchy bench:` lines on the console; T7.6's table; `gate.nnhs`, the replay's 60 Hz verdict, `prefabs bench: PASS` and `entities bench: PASS` on the console |
| AC-8 | stage 0 references compared after stages 2, 5 and 6; `hello` sizes; probes; build times |

## Risks and alternatives

1. **`pass_move` (0.65–1.0 ms, target 1.0) and `reparent` (0.8–0.95, target 1.0) at risk; `world_now` and `spawn`
   tight.** Mitigations in order, after the console says so: (a) the pass: a third `pld` or a farther distance, the
   finiteness test on the outputs folded into the stores, the root scan reading the parent field of a separate packed
   word; (b) `sin_quarter`'s clamp as an integer compare (no `vmrs`); (c) reparents: one reciprocal for the three
   divisions where the parent has no shear, the subtree walk skipped for leaves (child count 0); (d) `world_now`: the
   climb prefetching both lines of each ancestor. Then AC-7's measurement-with-margin policy.
2. **Keep-world uses stored worlds**, not fresh ones (decision 6 below): a node moved this frame before the pass is
   reparented from where it was last computed (what was on screen). Godot and Unity use fresh values; Bevy uses the
   last propagated one. The fresh form doubles the reparent cost; the docs say "attach after the pass, or call
   `world_now` first and write it back" for the other behaviour. Flagged for the maintainer.
3. **Subnormals**: the 3DS flushes them, the PC doesn't. Products below 2⁻¹²⁶ (scales or positions near 10⁻³⁸) may
   give different bits; documented as outside the determinism domain; the self-check and the fuzz targets' equality
   checks stay in normal numbers. The inverse's threshold is an exponent test, so a refusal never depends on it.
4. **Exactness up to the sign of zero**: `parent + local` and the root rule are exact as IEEE values; a zero may come
   out `+0` where a hand-written sum gives `−0` (documented; tests compare with `==` for zeros, bits otherwise).
5. **Moving `sin_quarter`** must not change a tween bit: the same code, the same `-ffp-contract=off`; T1.1 compares
   the 3ds-dev disassembly and spec 015's tests and `tweens: PASS B44CA3F6` (`--pc`).
6. **The prefab dispatch touches spec 021's spawn path** (one load and branch): `7BB7901E` unchanged, `prefab_spawn`'s
   count re-read in T7.6, and `prefabs bench: PASS` on the console (T10.2 run 4).
7. **`WorldCold` grows** (entities-internal): appended at its end, so spec 020's code that reads it is unchanged
   (`World::spawn` 109, `world_add` 143 byte-identical, T2.4); `C8FB9A41` and `entities bench: PASS` on the console
   (run 5).
8. **The platformer's lines change** (decision 24): if the maintainer prefers unchanged lines in the morning, stage 6
   is reverted as one commit (barrels out, lines back) and the hierarchy example carries everything (spec option c).
9. **Mirrored sprites and pivots**: gfx mirrors a negative scale *inside the sprite's rectangle*, so a mirrored node
   looks mirrored about its pivot only with a centred pivot: the docs say so and the example uses centred pivots.
10. **The walk cursor** is the most intricate code: covered by `test_each_descendant.cpp`, the fuzz target, mutation
    testing and ASan.
11. **Node copy semantics** (links never copied) may surprise a game copying nodes around: documented on the type.

## Stage 10: console run (prepared)

T10.1 on the PC first: `scripts/dkp.sh cmake --build --preset 3ds-release` and `scripts/dkp.sh cmake --build --preset
3ds-dev --target hierarchy_3dsx platformer_3dsx prefabs_3dsx entities_3dsx`, without a warning; T7.6's table still
describes the objects; 3ds-release `hello.elf` sizes equal T0.1's. Then on an Old 3DS, nobody touching it after Y. Five
runs, each: open the Homebrew Launcher, press **Y**, then on the PC (`nnh` builds the Development `.3dsx`, sends it,
plays the script and brings the console back to the Homebrew Launcher; logs and screenshots in `build/logs/`):

1. `build/pc-debug/tools/nnh/nnh run hierarchy --script examples/hierarchy/tests/check.nnhs` (≈ 1 min). Expected, as
   on the PC (pc-debug), with the values recorded in "Stage 5 results" and "Stage 7 results": `hierarchy: self-check
   started`, `hierarchy: 10000 sum <c1>` … `hierarchy: 50000 sum <c5>`, **`hierarchy: PASS <golden>`**; the showcase
   lines the script expects (`yard: restarted nodes <n>`, `picked: crate slot <s> gen <g> depth 0 at <x>,<y>`,
   `attached: crate to hand at <x>,<y>`, `dropped: crate at <x>,<y>` with the same `<x>,<y>`, `picked: hat slot <s> gen
   <g> depth 1 at …`, `detached: <n> children`, `destroyed: ship + <n> children`), then `hierarchy bench: started`,
   `hierarchy bench: pass_turn avg X ms worst Y ms (target < 1.5 ms) PASS|FAIL`, `pass_move … (target < 1 ms)`,
   `world_now … (target < 1.5 ms)`, `spawn … hand H ms ratio R (target < 2.2 ms, <= 1.3)`, `reparent … (target < 1
   ms)`, and the verdict `hierarchy bench: PASS` or `hierarchy bench: FAIL (<names>)`; `nnh: PASS (<n> lines, …)`.
   **Expected from the ARM pre-check** (T7.6 refines it): every logic line equal to the PC's; `pass_turn` 0.85–1.25 ms,
   `world_now` 0.95–1.1, `spawn` 1.6–1.75 with a ratio ≈ 0.75 — PASS; `pass_move` 0.65–1.0 and `reparent` 0.8–0.95
   — at risk. A FAIL fails the run (`fail-on "hierarchy bench: FAIL"`) after every number is logged: record them in
   "Console run 1"; T10.3 then applies risk 1's list, or the budgets are set from the measurement with margin. Read the
   screenshots (showcase, mirrored walker, carried crate, cascade, debug draw, bench): the same pictures as a `--pc`
   run's, rotated sprites within a pixel (rasterisation is the backend's).
2. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/level.nnhs` (≈ 40 s). Expected:
   exactly the 15 `replay: … sum` lines committed by T6.3 (x, y and coins columns as before spec 022, every sum new),
   `replay: done 900 frames sum <new>: 60 Hz PASS`; `nnh: PASS (79 lines, …)`. A different sum is the barrels' (run 1
   covers the module).
3. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/gate.nnhs` (≈ 2.5 min). Expected:
   `bench: PASS 4 layers + 300 sprites, …`; `nnh: PASS`.
4. `build/pc-debug/tools/nnh/nnh run prefabs --script examples/prefabs/tests/check.nnhs` (≈ 40 s). Expected, unchanged
   since spec 021: `prefabs: 10000 sum 3EBCC2FD`, `20000 … 821C8B95`, `30000 … CC194CA2`, `40000 … C8360424`, `50000 …
   7BB7901E`, **`prefabs: PASS 7BB7901E`**, every showcase line as in plan 021, and `prefabs bench: PASS` (spawn < 2
   ms ≤ 1.25×, overrides < 2.3, map < 4.5, find < 0.65 ms; numbers near spec 021's 1.62 / 1.81 / 3.58 / 0.50: the
   dispatch adds one load and a branch); `nnh: PASS (124 lines, …)`.
5. `build/pc-debug/tools/nnh/nnh run entities --script examples/entities/tests/check.nnhs` (≈ 45 s). Expected, unchanged
   since spec 020: `entities: 10000 sum 723FB297` … `100000 sum C8FB9A41`, **`entities: PASS C8FB9A41`**, `entities
   bench: PASS` with numbers near spec 020's (each 0.475, spawn 1.29, find 0.144, get 4.78, storage get 4.20, pool 0.441
   ms): the cold part grew at its end, the hot code is byte-identical (T2.4).

## Decisions taken (overnight, to review)

The spec's "Decisions to take" are approved as recommended. The spec's open questions and these plan-level decisions
were taken overnight, each the recommended or simplest option; the maintainer reviews them in the morning:

1. **Names**: modules `transform` (`<nnh/transform.hpp>`: `Transform2D`, `Affine2D`, `WorldTransform2D`, `SinCos`,
   `sin_cos`, `kQuarterTurn`/`kHalfTurn`/`kThreeQuarterTurn`/`kFullTurn`, `to_world`, `compose`, `transform_point`,
   `transform_direction`, `inverse`, `to_local`, `relative`, `bounds`, `is_finite`) and `hierarchy` (`<nnh/hierarchy.hpp>`:
   `Node`, `node_component`, `Hierarchy`, `Reparent::KeepWorld|KeepLocal`, `AttachCheck`, `HierarchyStats`, `propagate`,
   `world_now`, `world_transform`, `kMaxHierarchyDepth`); prefabs `PrefabChild`, `child()`, `spawn_under`; gfx
   `transformed` and a `draw` overload; example `examples/hierarchy` (`HIERARCHY_DEMO`).
2. **Open question 1 (accuracy):** FR-3's 1/32 px is confirmed as the composition bound (against a `double`
   reference using the engine's sine values) for depth ≤ 16, coordinates within ±4096 px, scales ¼–4; against exact
   trigonometry the sine's own error adds (≤ 7·10⁻⁷ rad per level); T1.6 measures both and the docs state both numbers.
3. **Open question 2:** depth limit **16** confirmed; node record **80 bytes** on every system.
4. **Open question 3:** the **sine cache is kept** (12 B per node); T10.2's `pass_move` is the evidence.
5. **The sine**: spec 015's `sin_quarter` moved to `src/core/det_sine` (bits unchanged); radians reduced by `× float(2/π)`
   to the nearest quarter, `|f| < 2⁻²⁰` snapped to 0 (exact constants and small multiples), non-finite angles → {0, 1},
   huge angles by an integer path.
6. **Keep-world reparenting uses stored worlds** (the node's and the new parent's last computed ones) and leaves the
   node's stored world unchanged; keep-local and detach keep-local recompute it.
7. **When a stored world is computed**: `Node(local)`, keep-local attach/detach, tree spawns (after placement), the
   pass. Writes to `local` (the game's, a tween's, a single-entity prefab's placement) show at the next pass and in
   `world_now`.
8. **A pure-translation local** (rotation `+0`, scale exactly (1, 1) by bits) composes by copying the parent's axes and
   drawing values: part of the definition, shared by every path.
9. **Exactness holds as IEEE values**; the sign of a zero may differ from a hand-written sum.
10. **Node copies never carry links**; assignment keeps the destination's links.
11. **`can_attach`** (no assert) is public, for scripts, UIs and the self-check; `attach` asserts on refusal (FR-6).
12. **A dying entity counts as stale** for attach (child or parent).
13. **Drawing merge under a mirror subtracts** the given rotation (consistent with composition; the spec says "adds"
    for the unmirrored case). Helpers: `gfx::transformed` and `gfx::draw(screen, texture, world, options)`.
14. **Non-finite locals**: the pass asserts (Debug/Development); Release treats that local as the identity for the
    pass and counts it (`bad_locals`).
15. **Stats roots, deepest and computed are as of the last pass** (no scan); refusals and bad locals since creation.
16. **Per-world module words** (`WorldModuleState`, 24 B) at the end of the world's cold part, for walks and stats.
17. **`world_reserve_many`** (entities-internal) for all-or-nothing trees.
18. **Prefab children**: `PrefabOptions::children` (≤ 8, made by `nnh::child()`, which links the tree code), the
    library capacity `max_children` (default 0 = spec 021's layout), ≤ 32 entities and ≤ 128 components per tree,
    `spawn_under(world, prefab, parent[, at])` without overrides (a follow-up if needed), depth refusals counted as
    misses under `WhenFull`, `place(&Node::local)` accepted.
19. **Children ranges** read the next sibling before yielding.
20. **The pass is `propagate()`** (Bevy's word; "update" is taken by scenes).
21. **Example layout and controls** as described above (Old 3DS controls only: D-Pad aims; the C-Stick is not needed).
22. **Platformer**: barrels as child entities with their own `turret.png`, the turret's node placed by its hook (the
    collision box stays the map rectangle), the turret state hashing the barrels' worlds, the replay lines regenerated
    by the procedure above.
23. **Console stage**: five scripted runs (hierarchy `check.nnhs` with the bench; platformer `level.nnhs` with the new
    lines; `gate.nnhs`; prefabs `check.nnhs` for the dispatch; entities `check.nnhs` for the grown cold part).
24. **AC-7 targets kept as written** (the maintainer's rule); two flagged at risk.

## Constitution Check (after design)

| Article | OK? | Note |
|---------|-----|------|
| I | ✅ | Portable core; one fixed sequence of float operations per path, shared inline code for the pass, `world_now` and `compose`; libm-free sine with defined edge cases; integer sign/finiteness tests; AC-2 on every compiler and the console; subnormal and x87 limits documented |
| II | ✅ | 80 B per node, 24 B per world; a stackless, prefetched pass over the node array only; no per-node calls, `vmrs`, `__aeabi_*` or `memcpy`; keep-world without a sine; disassembly checked before the console; two targets at risk with AC-7's fallback |
| III | ✅ | A math-only transform header; a hierarchy header over `entities.hpp` with two small templates; measured budgets |
| IV | ✅ | No events |
| V | ✅ | No allocation after creation, no `virtual`, no RTTI, no exceptions; the pass calls no game code |
| VI | ✅ | Tests first; a `double` reference; twin worlds; AC-2 golden; the replay lines regenerated deliberately and required on the console |
| VII | ✅ | Two modules of small files, one public header each |
| VIII | ✅ | Two API pages, linked pages, example, platformer, D37 |
| IX | ✅ | Own code, prior art cited, original art |
| X | ✅ | No parser; misuse loud in Debug, harmless in Release; non-finite locals caught at the pass |
| XI | ✅ | The node kind is registered by the game; spec 020's public header unchanged; prefabs without children link no tree code; transforms link no entities code; `hello` unchanged |

No constitution exception is requested. D37 is new (see [decisions.md](../../docs/architecture/decisions.md)); D35
and D36 get dated notes (entities-internal additions; prefab children and the dispatch); D30 is not amended (the pass
is the game's call); the patterns page gains a "Transform hierarchy (Composite)" row; the scenes page notes the naming.
Two AC-7 targets are flagged at risk and keep the spec's measurement-with-margin fallback.

## Stage results and refinements

### Stage 0 results

Windows, WinLibs GCC (pc presets) and devkitARM (3DS presets), 2026-11-23, at commit `7c9e3a5`. Scripts in
`build/ref-022/` (not committed; adapted from `build/ref-021/`).

- **`hello` sizes (bytes)**: pc-release `hello.exe` 278 856 (`text` 130 572), pc-debug 1 284 282 (`text` 198 464);
  3ds-release `hello.elf` 2 482 372 (`text` 201 012), `.3dsx` 246 400; 3ds-dev `hello.elf` 2 740 628 (`text`
  217 728), `.3dsx` 267 844. `nm -C` lists saved (`hello.pc.nm`, `hello.3ds.nm`).
- **`.text` (3ds-release)**: platformer 361 568, prefabs 343 524, entities 271 024.
- **Build times** (3 runs, centiseconds): edit `examples/prefabs/room.cpp` → pc-debug `prefabs` 231 / 219 / 223,
  3ds-release `prefabs_3dsx` 507 / 500 / 511; clean `nnh_core` pc-debug 10.5 s.
- **Header cost** (one-line TU, `-E` lines and `-fsyntax-only`): `<nnh/entities.hpp>` 8 555 lines, 0.45–0.51 s;
  `<nnh/math.hpp>` 63 lines, 0.26–0.31 s.
- **3ds-dev disassembly** (`dis0/`, `fcount.py` totals per function): `World::spawn` 109, `detail::world_add` 143,
  `detail::prefab_spawn` 491 (64 stack accesses), `detail::sin_quarter` 19 (1 `vmrs`: the clamp), `sin_turns` 86 with
  `sin_quarter` inlined (no call), `examples/entities` `bench.o` saved for byte comparison.
- **Goldens**: replay ctest `sum B2418A28: 60 Hz PASS`; `entities: PASS C8FB9A41`; `prefabs: PASS 7BB7901E`;
  `tweens: PASS B44CA3F6` (each `--pc` script PASS). `nnh_tests`: 2316 tests, `kMaxTests` 4096 (room left).
- **Reference screenshots** (`ref/`): hello, sprites, scenes, camera zoom1, queries all, entities, prefabs, platformer
  bench (top), level 1 at 90, replay at 64 and 312.

### Stage 1 results

- **T1.1**: `sin_quarter` moved to `src/core/det_sine.{hpp,cpp}`. Its body is an `always_inline` function
  (`sin_quarter_inline`) in the header, which `tween/ease_math.cpp`'s `sin_turns` uses (it was inlined there before), and
  `det_sine.cpp` defines the out-of-line `sin_quarter`. 3ds-dev disassembly: `ease.o` identical; `ease_math.o` identical
  except that `sin_quarter`'s definition left it; the moved body in `det_sine.o` is byte-identical (19 instructions, the
  clamp's `vmrs`). `tweens: PASS B44CA3F6` (ctest `tweens_self_check_pc`).
- **Refinement (T1.2)**: `sin_cos` lives inline in `src/transform/sin_cos.hpp` (`sin_cos_inline`), the snap writes
  `0 · f` (the sign of `f` kept, so `sin_cos(−x)` is `sin_cos(x)` with the sine's sign bit flipped for every input,
  zeros included); the public `sin_cos` adds the non-finite assert. The constant is `float(2/π)` = `0x3F22F983`.
- **Refinement (T1.3)**: the rules are inline in `src/transform/compose.hpp` (`root_world`, `translate_world`,
  `full_world`, `child_world`), shared by `compose()`, the pass and `world_now`.
- **Refinement (T1.4)**: `inverse`/`to_local` also refuse a non-finite determinant (exponent field 255), and
  `to_local` subtracts the origin before applying the inverse axes (better accuracy than going through `inverse`).
- **Refinement (T1.5)**: `bounds` has a second exact case besides unrotated maps: a map whose diagonal is zero (an
  odd number of quarter turns) swaps the axes exactly.
- **T1.6 accuracy** (2000 seeded chains, depth 1..16, world coordinates within ±4096 px, accumulated scales ¼..4,
  mirrors, any angles): worst world-position error **1.65·10⁻⁴ px** against a `double` reference using the engine's
  sine values (FR-3's bound 1/32 px confirmed with a margin of ≈ 190×), **2.5·10⁻³ px** against `<cmath>`'s sine (the
  polynomial's own error, < 2·10⁻⁶ rad per level, dominates). The tests bound them at 1/32 and 1/16 px.
- **T1.7**: `<nnh/transform.hpp>` preprocesses to itself and `math.hpp` only (140 lines, `-fsyntax-only` 0.27–0.34 s,
  the same as `math.hpp`'s 0.26–0.31 s); it has no inline float arithmetic. Guards: pc-debug ctest 268/268, pc-release
  258/258, `nnh_tests` 2353, 3ds-release and 3ds-dev without a warning, `hello` sizes and `nm` unchanged, `World::spawn`,
  `world_add`, `prefab_spawn` and `examples/entities` `bench.o` byte-identical.

### Stage 2 results

- **T2.1**: `WorldModuleState` is the last member of `WorldCold` (`modules`); `world_module_state`, `world_dying`,
  `world_busy(const World&)` in `src/entities/world_module.cpp`. `world_internal.hpp` now includes
  `entities_internal.hpp` (for the struct).
- **T2.2 refinements**: the group's kind ranges are consecutive from 0 (`ReserveEntity::first_kind`), so the room
  check touches only the kinds the group names (no 32-int array clearing: an earlier draft's two `memset` calls are
  gone); free slots are counted from each table's hint with `x &= x − 1` (no popcount call into libgcc). Room is decided
  exactly as `count` `world_reserve()` calls would (entity i needs the (i+1)-th free entity slot, then one more slot of
  each of its kinds, in order); the refusal is counted on that call's counter and nothing else changes (not even a
  hint). Proven on 20 seeded sequences of 2000 operations against one-by-one `world_reserve()` (`same_state` every 64
  steps), plus refusals, hooks and iterations; a mutant of the room check is killed.
- **T2.4 ARM guard** (3ds-dev, `dis2/` vs `dis0/`): `examples/entities` `bench.o`, `world_spawn.o` (`World::spawn` 109),
  `world_components.o` (`world_add` 143), `world_destroy.o`, `world_tags.o`, `world_flush.o`, `tags_find.o`,
  `world_reserve.o`, `world_lifetime.o`, `world_scene.o`, `pool_core.o`, `prefab_spawn.o`, `prefab_map.o`:
  byte-identical. New: `world_reserve_many` 294 instructions, `fits` 113, `free_slots` 34; no call, `vmrs`,
  `__aeabi_*` or `memset`.
- Guards: pc-debug ctest 268/268, pc-release 258/258, `nnh_tests` 2360 (pc-release 2320), 3ds builds without a warning,
  `hello` sizes and `nm` unchanged; entities, prefabs, tweens self-checks and the replay ctest pass.

### Stage 3 and 4 results (committed together: the hierarchy's links and its pass are one module)

- **Refinements**: `ChildRange` lives in `<nnh/hierarchy.hpp>` after `Node` (its iterator needs the complete type; the
  step to the next sibling is out of line); `hierarchy_walk.hpp` holds the link sentinel, `node_removed` and
  `DescendantWalk` (its iteration guard is a member). `Hierarchy` keeps the world pointer, a `Storage<Node>` and the
  node kind index. Internal code works through `detail::Tree` (world, block, kind, node array) and `NodeAccess`.
  `node_compute.hpp` holds the one inline node computation shared by the pass and `world_now` (finiteness by bits, the
  cache compared on the rotation's bits — `world_now` reads it but never writes it). `is_finite_bits` is inline in
  `transform/sin_cos.hpp`. `gfx::transformed`/`gfx::draw(…, WorldTransform2D, …)` are in `src/gfx/draw_transform.cpp`,
  listed with the hierarchy's `-ffp-contract=off` sources.
- **Walk rule (T4.3)**: besides the plan's "a remembered next sibling that is unlinked moves on to its next", an append
  under a node of the walk's path whose level has no next sibling left becomes that level's next (so a node moved after
  the current one is visited, as the plan says); every step re-checks the path from the top (a node destroyed or moved
  out is not descended into); fresh nodes (the node kind's fresh bits) are skipped with their subtrees.
- **Cascade (T4.4)**: the engine hook unlinks every child (`unlink`, which tells the walks) before destroying it
  (queued) or making it a root; tests prove breadth-first hooks with the last worlds intact and `parent(child)` null.
- **Pass tests (T4.1)**: no call log or sine counter was added to engine code: the order is proven by a child placed
  in a lower slot than its parent (a slot-order pass would read a stale world), each-node-once by the computed count,
  and the cache by a planted cached sine (read while the rotation's bits are unchanged, refreshed when they change).
- **Misuse (T4.8)**: `events::Stats` has no publish counter; the no-event test compares its fields, and the module
  includes no event header.
- **T3.5**: `<nnh/hierarchy.hpp>` pulls 60 headers, none of gfx, collision, camera, tween, animation, tilemap or
  prefabs; 8 980 preprocessed lines (+425 over `entities.hpp`), `-fsyntax-only` 0.47–0.50 s against `entities.hpp`'s
  0.48–0.52 s on the same run (within the 0.05 s budget). The header is 237 lines (comments included).
- **3ds-dev**: `propagate` 585 instructions in all, `world_now` 490 (4 `vmrs` each: `sin_quarter`'s clamp, inlined twice
  per rule; decided at T7.6); `examples/entities` `bench.o`, `World::spawn`, `world_add`, `world_destroy` and
  `prefab_spawn` byte-identical.
- **Guards**: pc-debug ctest 268/268, pc-release 258/258, `nnh_tests` 2414, 3ds-release and 3ds-dev without a warning,
  `hello` sizes and `nm` unchanged, every reference screenshot identical (T4.9).
