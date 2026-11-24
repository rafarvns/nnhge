# hierarchy — parents, children and world transforms

Header: `<nnh/hierarchy.hpp>` · Spec: [022](../../specs/022-scene-tree-and-transforms/spec.md) ·
Plan: [022](../../specs/022-scene-tree-and-transforms/plan.md) · Decision: [D37](../architecture/decisions.md) ·
Example: [`examples/hierarchy`](../../examples/hierarchy/yard.cpp) · In a game:
[`examples/platformer`](../../examples/platformer/turrets.cpp) · Transforms: [transform.md](transform.md) ·
Worlds: [entities.md](entities.md)

> Naming: the ROADMAP calls this the "scene tree" (Godot's word). In NNHGE a **scene** is a game mode on the scene stack
> ([scene.md](scene.md)), so this module says **hierarchy**, **node** and **transform**.

An entity takes part in a hierarchy through the engine's one component kind, **`nnh::Node`**: a **local transform** the
game writes, the **world transform** last computed for it, and links to its parent and children in the same world. A
gun on a ship, a hat on a head or a rider on a cart follows its parent — turned, scaled and mirrored with it — without
the game writing trigonometry. One **pass** per frame computes every world transform with the same bits on PC and 3DS;
`world_now` gives a fresh one mid-frame; attaching and detaching keep the world or the local transform; destroying a
parent destroys its children. No events, no allocation while playing.

Optional (constitution XI): a game that doesn't include the header links none of it; entities without a node and worlds
without the kind pay nothing. The header includes `<nnh/entities.hpp>` and `<nnh/transform.hpp>` only.

```cpp
const nnh::ComponentDesc kinds[] = {nnh::node_component(64), nnh::component<Sprite>(64)};
nnh::World world = std::move(nnh::World::create({.max_entities = 64}, kinds).value()); // check it in real code
nnh::Hierarchy tree(world);

const nnh::Entity ship = world.spawn(), gun = world.spawn();
world.add<nnh::Node>(ship, nnh::Node({.position = {100, 60}}));
world.add<nnh::Node>(gun, nnh::Node({.position = {12, -4}}));
tree.attach(gun, ship, nnh::Reparent::KeepLocal);   // the gun rides on the ship, 12 px ahead

// Each frame: move things, then the pass, then the camera, then draw.
tree.local(ship)->rotation += 0.05f;
tree.propagate();
world.each<nnh::Node, Sprite>([](nnh::Entity, nnh::Node& n, Sprite& s) {
    nnh::gfx::draw(nnh::Screen::Top, s.texture, n.world(), {.pivot = {8, 8}});
});
```

## Nodes and the node kind

Register the kind like any other: `nnh::node_component(capacity)` — or `node_component(capacity, hook)` with your own
remove hook (≤ 16 bytes of captures), which runs **before** the engine's while the node is still linked. A `Node` is 80
bytes on every system (links as 16-bit node slots, its depth and child count, the local, a cached sine and cosine, the
world). `Node(local)` computes its world at once (a root's). **Copying a Node copies its transforms, never its links**:
`world.add<Node>(e, other)` makes a root; assigning keeps the destination's links.

`Hierarchy` is a cheap view of a world's nodes (the kind looked up once): `nnh::Hierarchy tree(world)`. A view of a world
without the node kind asserts and answers nothing. Every operation is allocation-free and per-frame safe.

## Attach and detach

| Call | Effect |
|------|--------|
| `attach(child, parent, mode = KeepWorld)` | `child` becomes `parent`'s last child |
| `can_attach(child, parent, mode)` | what `attach` would answer: `AttachCheck::Ok` or the refusal, no assert |
| `detach(child, mode = KeepWorld)` | `child` becomes a root |
| `detach_children(e, mode = KeepWorld)` | every child of `e` becomes a root; returns how many |

**Modes.** `Reparent::KeepWorld` (the default, Godot's `reparent`, Unity's `SetParent`): the node stays where it is on
screen — its stored world is unchanged and its local becomes `relative()` to the new parent's **stored** world (a held
item picked up or dropped doesn't jump). `Reparent::KeepLocal`: the local is kept and the node moves to its place under
the new parent (its world is computed now). Descendants keep their locals (their stored worlds update at the next pass).
Keep-world uses the **stored** worlds — what was last computed, what was drawn: to reparent from where a node moved this
frame, call `propagate()` (or write `world_now` into it) first.

**Refusals**, in this order — `attach` returns false, asserts in Debug/Development, counts `stats().refusals` and
changes nothing: `Stale` (a null, stale or dying entity, child or parent), `NoNode`, `Cycle` (the child is the parent
or one of its ancestors), `TooDeep` (the child's subtree would reach depth 16: a root is depth 0, the deepest node depth
15, `kMaxHierarchyDepth` levels), `NotInvertible` (keep-world under a parent whose map can't be inverted: a zero scale).
Attaching to the current parent changes nothing (true). Children keep attach order; there is no reordering (detach and
attach again to move one to the end). Handles carry no world id: a handle of another world whose slot is valid here is
not detected.

## Queries

`parent(e)`, `depth(e)` and `child_count(e)` are constant time; `root(e)` climbs; `first_child(e)`, `next_sibling(e)` and
`children(e)` (`for (nnh::Entity c : tree.children(ship))`, attach order; destroying or detaching the current child
inside is safe) walk the links; `is_ancestor(a, e)` (strict); `local(e)`, `node(e)`, `world_transform(e)` (the stored
one; null without a node). A stale handle or an entity without a node answers null, −1, 0 or false.

`each_descendant(e, fn)` visits a subtree pre-order, children in attach order, with spec 020's iteration rules:
destroying, detaching, attaching and spawning inside are safe — what is destroyed or moved out is not visited, what is
moved into the part not yet walked is, what is created during the walk is not.

## The pass and `world_now`

`propagate()` computes every node's world: roots in ascending node slots, each subtree depth-first in child order, each
node once from its parent's fresh world and its own current local. It is **your call** — normally once per frame:

```
input → game update (move roots, aim guns, tweens) → tree.propagate() → bodies from bounds → camera rig → draw
```

It calls no game code, walks only the node array (prefetching ahead), recomputes a node's sine only when its rotation's
bits changed, and skips the sine for pure translations. Inside an iteration of the same world or a remove hook it
asserts and does nothing. A non-finite local asserts; Release treats it as the identity for that pass and counts it in
`stats().bad_locals`. It is not run by scene binding (no new scene notification, decision D37).

A node's **stored** world is the one from the last pass, or — for a node created, attached keep-local or detached
keep-local since — the one computed at that moment; keep-world reparenting leaves it unchanged. `world_now(e)` computes
a fresh one from the current locals of `e` and its ancestors, **bit for bit** what a pass would store now, and stores
nothing: a bullet leaves the muzzle in the frame the gun turned —
`nnh::transform_point(tree.world_now(muzzle).map, {0, 0})`.

## Destroying, removing, clearing

**Destroying a parent destroys its children** through spec 020's destroy queue: breadth-first in child order, no
recursion; each descendant runs its own remove hooks with its components intact (its node's last world included: "an
explosion where the gun was"), its parent already stale and `parent(child)` null. To keep children, detach them first:
`tree.detach_children(cart)` (keep-world: a rider jumps off where it was). **Removing** the node component
(`world.remove<nnh::Node>(e)`) detaches `e` and makes its children roots that keep their stored worlds (nothing is
destroyed). `world.clear()` destroys every node; cascades merge into it; a level rebuilt after a clear gets its first
load's slots. A world bound to a scene flushes and clears its nodes with it (spec 020's binding); node slots are
quarantined until the flush like any component's.

## Drawing, collision boxes, the camera

**Drawing.** `gfx::draw(screen, texture, node.world(), options)` draws at the world position with the world's rotation
added and its scale multiplied into `options` (region, pivot, flips, tint, opacity, layer kept; an animator's
`draw_options()` works as given options); `gfx::transformed(world, options)` gives the merged options. The engine ships
no sprite component: draw your nodes in your own loop (`world.each<nnh::Node, Sprite>`). Centre pivots on mirrored
sprites. Rotated texels are each backend's rasterisation (spec 004): compare logged values, not those pixels.

**Particle emitters that follow a node.** After the pass, `emitter.set_transform(node.world())` then
`emitter.update()`: a world-space emitter leaves a trail behind the node, a local-space one carries its particles with
it, turned and mirrored ([particles.md](particles.md#world-and-local-space-following-things)). The platformer's muzzle
flashes start at a barrel's stored world transform this way.

### Collision boxes that follow

`nnh::bounds(world.map, local_box)` is the axis-aligned box of a local rectangle under a world transform (the enclosing
box when rotated). The pattern: after the pass, write it into your body component, so spec 019's box views and queries
read children's boxes in place:

```cpp
tree.propagate();
world.each<nnh::Node, Body>([](nnh::Entity, nnh::Node& n, Body& b) { b.box = nnh::bounds(n.world().map, b.local); });
```

A root driven by physics (`collision::move` on its body) copies its box position into its local position before the
pass ("physics drives the root, the hierarchy drives the children").

**Camera.** A node's world position after the pass is a point `CameraRig::update` follows (the rider, not the cart):
update → pass → rig → draw. No camera change.

## Prefabs with children

A prefab may list child prefabs — `.children = {nnh::child(gun, {.position = {12, -4}})}` — and one spawn makes the whole
hierarchy all or nothing, the hooks running children first; `spawn_under(world, prefab, parent[, at])` spawns one under
an existing node. See [prefabs.md](prefabs.md#children).

## Stats and debug drawing

`stats()`: `nodes`, `peak`, `capacity` (the kind's), `roots`, `deepest` and `computed` (as of the last pass), `refusals`
and `bad_locals` (since creation) — a debug overlay line. `debug_draw(screen)` (Debug and Development only; nothing in
Release) draws a line from each node to its parent and its local axes (8 px).

## Costs

| Item | Size or cost |
|------|--------------|
| per node | 80 B + 2.5 B of the kind's bookkeeping (1024 nodes ≈ 82.5 KB in the world's block) |
| per world | 24 B of module words (every world, used or not) |
| `Hierarchy` view | ≈ 36 B (3DS) / 56 B (PC) |
| `parent`, `depth`, `child_count`, `world_transform` | constant time |
| `propagate` | linear in the nodes; a sine only when a rotation changed |
| `world_now` | linear in the depth (≤ 16 ancestors, 32 B of stack) |
| attach | linear in the moved subtree (its height and depths); keep-world: no sine, three divisions |
| cascades | one queued destroy per descendant |

Measured on an Old 3DS XL (Development build, after spec 022's optimisation round; the budgets are these plus ≈ 25 %):

| Workload | Old 3DS | Budget |
|----------|---------|--------|
| pass, 1024 nodes, every root turned | 1.17 ms | < 1.45 ms |
| pass, 1024 nodes, positions only | 0.81 ms | < 1.0 ms |
| 256 `world_now`, depth 3 | 1.11 ms | < 1.4 ms |
| 64 prefab hierarchies of 4 spawned + destroyed | 2.89 ms (the same by hand: 2.92) | < 3.6 ms, ≤ 1.3× by hand |
| 512 reparents | 0.92 ms | < 1.15 ms |

So a frame's pass costs about 1 µs per node, and a prefab hierarchy costs the same as the hand-written spawn, add,
add_tag and attach calls (both mostly the world's memory traffic once its arrays outgrow the 16 KB cache). Run
`nnh run hierarchy --script examples/hierarchy/tests/check.nnhs` to measure on your console (spec 022's stage 10).

## Misuse

Loud in Debug/Development, harmless in Release (answers nothing or the identity, changes nothing): a stale, null or
(when detectable) foreign handle; an entity without a node; cycles; too deep; keep-world under a non-invertible parent;
a non-finite local reaching the pass; the pass inside an iteration of the same world or from a hook; a `Hierarchy` of a
world without the node kind; prefab children without the node kind (registration errors).

## Pitfalls

- The pass is yours: write locals, then `propagate()`, then read worlds. `world_transform()` between passes is last
  frame's for a node you moved; use `world_now()` when you need this frame's.
- Mirror with `scale.x = −1` on the root and centre the pivots.
- Copies of a `Node` carry no links.
- A single-entity prefab placed through `&Node::local` gets the position in its local; its stored world is the stored
  value's until the next pass (spec 021's spawn path is unchanged). Prefab hierarchies compute every world at spawn.
