# transform — 2D transforms as deterministic values

Header: `<nnh/transform.hpp>` · Spec: [022](../../specs/022-scene-tree-and-transforms/spec.md) ·
Plan: [022](../../specs/022-scene-tree-and-transforms/plan.md) · Decision: [D37](../architecture/decisions.md) ·
Hierarchies of entities: [hierarchy.md](hierarchy.md) · Values: [math.md](math.md)

A **local transform** (`Transform2D`) says where something is relative to its parent: a position, a rotation and a
scale. Composing a parent's **world transform** with a child's local gives the child's world transform: an **affine
map** (exact for points and directions, even under shear) plus **drawing values** (an accumulated rotation and a
product of scales) that `gfx::draw` takes. The math — composition, points, directions, inverses, keep-world
reparenting, axis-aligned bounds and the engine's **sine and cosine** — gives the **same bits on PC and 3DS**. Nothing
allocates; no libm.

These are plain values: a game with its own arrays (pools, bullets, a boss made of parts) composes its own structs
without a world. The hierarchy ([hierarchy.md](hierarchy.md)) is built on them. The header includes only
`<nnh/math.hpp>`, and every float operation lives in the engine's sources (compiled without fused multiply-add), so a
game's compiler flags can't change a bit. A game using only this header links no entities code.

```cpp
const nnh::Transform2D ship{.position = {100, 60}, .rotation = nnh::kQuarterTurn};
const nnh::Transform2D gun{.position = {12, -4}};               // relative to the ship
const nnh::WorldTransform2D ship_w = nnh::to_world(ship);
const nnh::WorldTransform2D gun_w = nnh::compose(ship_w, gun);    // at (104, 72): turned with the ship
const nnh::Vec2 muzzle = nnh::transform_point(gun_w.map, {10, 0}); // where a bullet starts
nnh::gfx::draw(nnh::Screen::Top, gun_texture, gun_w, {.pivot = {8, 8}});
```

## Values

| Type | Fields | Size |
|------|--------|------|
| `Transform2D` | `position` (px), `rotation` (radians), `scale` (default (1, 1)) | 20 B |
| `Affine2D` | `x`, `y` (images of the unit axes), `origin` | 24 B |
| `WorldTransform2D` | `map` (an `Affine2D`), `rotation`, `scale`, `position()` = `map.origin` | 36 B |
| `SinCos` | `sin`, `cos` | 8 B |

**Units and order.** Angles are radians, **clockwise on the y-down screen** — the unit of `DrawOptions::rotation`. A
local transform scales, then rotates, then moves, about its own origin: `p ↦ position + R(rotation)·S·p`. A negative
scale component mirrors on that axis. Defaults are the identity. `WorldTransform2D` is deliberately not an aggregate
(a braced `{x, y}` keeps meaning a `Vec2` position in `gfx::draw`).

## Functions

| Call | What it gives |
|------|---------------|
| `sin_cos(radians)` | the engine's sine and cosine (below) |
| `to_world(root)` | a root's world: its drawing values are `root`'s bits, its map's origin its position |
| `compose(parent, local)` | a child's world under a parent's world |
| `transform_point(map, p)` / `transform_direction(map, d)` | a local point / direction in world space |
| `inverse(map, out)` | the inverse map; false when \|determinant\| < 2⁻¹⁰⁰ or not finite (`out` untouched) |
| `to_local(map, world_point, out)` | a world point in the map's space; false as `inverse` |
| `relative(parent, world, out)` | the local that puts a node at `world` under `parent` (keep-world reparenting) |
| `bounds(map, rect)` | the axis-aligned box of a local rectangle (below) |
| `is_finite(t)` | false when any of the five floats is infinite or NaN (an integer test on the bits) |

Constants: `kQuarterTurn` (`float(π/2)`), `kHalfTurn`, `kThreeQuarterTurn`, `kFullTurn`.

## Determinism and exactness

- **Same bits everywhere.** Only IEEE `+ − ×` (and the inverse's three divisions), float↔int conversions and integer
  tests on float bits (signs, mirrors, finiteness, the inverse's threshold), in a fixed order, compiled with
  `-ffp-contract=off`. The tests and the [hierarchy self-check](../../examples/hierarchy/self_check.cpp) hold golden
  bits on every host compiler and on the 3DS. Two limits: the 3DS flushes subnormal floats to zero (the PC doesn't), so
  values whose products fall below 2⁻¹²⁶ (≈ 10⁻³⁸) may differ; 32-bit x86 hosts (x87) aren't supported.
- **The sine.** `sin_cos(a)`: `q = a · float(2/π)`, `n` the nearest whole, `f = q − n` (exact); `|f| < 2⁻²⁰` snaps to a
  whole quarter; the quadrant `n & 3` picks `±sin_quarter(|f|)` and `±sin_quarter(1 − |f|)` (spec 015's degree-7
  polynomial, shared with the easings). Error < 2·10⁻⁶ against the exact sine. **Quarter turns are exact**:
  `k · kQuarterTurn` for small `k` gives sine and cosine of exactly 0 and ±1, so a child turned 90° lands on whole
  pixels. `sin_cos(−a)` is `sin_cos(a)` with the sine's sign flipped. A non-finite angle gives `{0, 1}` and asserts in
  Debug/Development; angles of any size have defined bits (no float→int overflow).
- **Exact identities.** A root's world equals its local bit for bit. A local with rotation `+0` and scale exactly
  (1, 1) is a **pure translation**: the parent's axes and drawing values are kept and the child sits at exactly
  `parent + local` position (one addition per axis) under an unrotated, unscaled parent. The identity's world is exactly
  `WorldTransform2D{}` (no −0). Exactness is as IEEE values: a zero may come out `+0` where a hand-written sum gives
  `−0`.
- **Accuracy.** Measured over 2000 seeded chains (depth up to 16, world coordinates within ±4096 px, accumulated scales
  ¼..4, mirrors, any angles): world positions within **1.7·10⁻⁴ px** of a `double` reference using the engine's sine
  values (the bound FR-3 promises is 1/32 px), and within **2.5·10⁻³ px** of one using exact trigonometry.

## Mirroring and shear

A negative scale mirrors a node and its whole subtree: children's positions are mirrored, their rotations turn the other
way on screen (the drawing rotation subtracts a mirrored child's rotation) and the drawing scale carries the flip.
`DrawOptions::flip_x`/`flip_y` stay per sprite and are not inherited. One `scale.x = −1` turns a character around with
everything it carries. Mirrored sprites are mirrored inside their rectangle about the pivot: **centre the pivot** so a
mirrored sprite stays in place.

A non-uniform scale on a parent of a rotated child produces **shear**. The affine map represents it (every point is
exact); the drawing values can't (a sprite is drawn with a rotation and a scale): they are the accumulated rotation and
the product of scales — Unity's `lossyScale` approximation. Without shear (uniform scales, mirrors, or non-uniform
scales whose descendants aren't rotated relative to them) the drawing values are exact.

## Points, inverses, keep-world

`inverse` and `to_local` refuse maps whose determinant's exponent is below 2⁻¹⁰⁰ (a zero scale somewhere) or not finite:
an exponent test, the same on every system. `relative(parent, world, out)` computes the local that reproduces `world`
under `parent` — position through the parent's inverse (within FR-3's bound), rotation and scale from the drawing values
(exact without shear; for power-of-two scales and quarter turns the round trip `compose(parent, relative(...))` gives
back the drawing values bit for bit). It needs no sine.

Particle emitters take these values as their place: `ParticleEmitter::set_transform(Transform2D)` or
`(WorldTransform2D)`; a local-space emitter draws its particles through the map by the mirror rule above
([particles.md](particles.md#world-and-local-space-following-things)).

## Bounds

`bounds(map, rect)` is the axis-aligned box of a local rectangle (a collision box) under a map: an unrotated map gives
the mapped rectangle (`x = origin.x + a·r.x`, `w = |a|·r.w`; mirrored rectangles normalised to a positive size), an odd
number of quarter turns swaps the axes exactly, and any other rotation gives the box enclosing the four mapped corners.
Spec 019's queries stay axis-aligned: a rotated child's box is its enclosing box ([hierarchy.md](hierarchy.md#collision-boxes-that-follow)).

## Costs

Every call is constant time and allocation-free. On the Old 3DS a composition is ≈ 30 VFP operations, the sine ≈ 40
instructions (the hierarchy caches it per node); see [hierarchy.md](hierarchy.md#costs) for the measured pass.
