# math3d — 3D math as deterministic values

Header: `<nnh/math3d.hpp>` · Spec: [026](../../specs/026-3d-foundation/spec.md) ·
Plan: [026](../../specs/026-3d-foundation/plan.md) · Decision: [D44](../architecture/decisions.md) ·
Drawing: [gfx3d.md](gfx3d.md) · Meshes: [mesh.md](mesh.md) · 2D values: [math.md](math.md), [transform.md](transform.md)

Vectors, quaternions, 4×4 matrices, 3D transforms, rays, look-at and projections — plain values usable without
graphics (a 2D game can use them for physics). Every operation uses only IEEE `+ − × ÷`, a correctly rounded square
root and the engine's `sin_cos` ([transform.md](transform.md)), in a fixed order, compiled without fused multiply-add:
**the same inputs give the same bits on PC and 3DS**. Nothing allocates; no libm.

```cpp
const nnh::Transform3D ship{.position = {0, 1, 0}, .rotation = nnh::quat_axis_angle({0, 1, 0}, nnh::kQuarterTurn)};
const nnh::Mat4 model = nnh::to_mat4(ship);
const nnh::Vec3 nose = nnh::transform_point(model, {0, 0, -1}); // (-1, 1, 0): turned left about +y
```

## Conventions

- Right-handed, **+y up**, a camera looks down its **−z** (glTF's and OpenGL's: models need no conversion).
- Angles in **radians**; a positive angle turns counter-clockwise seen from the tip of the axis.
- Matrices act on **column vectors** (`M × v`, `a * b` = `a` after `b`) and are stored **column-major**:
  `m[column * 4 + row]`, `at(row, column)`. The translation is `m[12..14]`.
- Projections give OpenGL clip space (depth −1 at the near plane, +1 at the far one). The 3DS's depth range and screen
  rotation are the backend's business: no public value depends on them.

## Values

| Type | Fields | Notes |
|------|--------|-------|
| `Vec3` | `x, y, z` | `+ − × ÷` by a scalar, `+=`, `==` inline (one IEEE operation per component) |
| `Vec4` | `x, y, z, w` | homogeneous coordinates |
| `Quat` | `x, y, z, w` | rotation; the default is the identity; need not be unit (`to_mat4` divides by \|q\|²) |
| `Mat4` | `m[16]` | the default is the identity; `identity()`, `at(row, col)` |
| `Transform3D` | `position`, `rotation`, `scale` | scaled, then rotated, then moved; `to_mat4` = T × R × S |
| `Ray3` | `origin`, `direction` | made by `gfx3d::screen_ray` |

## Functions

| Function | Does |
|----------|------|
| `dot`, `cross`, `length`, `length_squared` | `cross(x, y) = z` |
| `normalize(Vec3)` | unit length, each component divided by the length (axes stay exact); a length² below 2⁻¹⁰⁰ or not finite gives (0, 0, 0) and asserts |
| `quat_axis_angle(axis, radians)` | the axis is normalised inside; half angles of an odd multiple of 45° get bit-identical sine and cosine magnitudes |
| `quat_euler(yaw, pitch, roll)` | Y(yaw) × X(pitch) × Z(roll) |
| `look_rotation(forward, up)` | −z along `forward`, +y as close to `up` as possible; `forward` parallel to `up` uses up = (0, 0, ∓1): looking straight down, north (−z) is at the top |
| `a * b`, `conjugate`, `normalize(Quat)`, `rotate(q, v)` | `rotate` is `to_mat4(q)` applied to `v` |
| `nlerp`, `slerp` | shortest arc; `slerp` by Eberly's polynomial (16 terms): exact ends, error 1.2·10⁻⁷ |
| `to_mat4(Quat)`, `to_mat4(Transform3D)` | each entry a numerator over \|q\|²: quarter turns give exactly 0 and ±1 |
| `Mat4 * Mat4`, `Mat4 * Vec4`, `transpose` | sums in the fixed order ((a₀ + a₁) + a₂) + a₃ |
| `inverse`, `inverse_affine` | false (`out` untouched) when \|det\| < 2⁻¹⁰⁰ or not finite; `inverse_affine` for matrices with a (0, 0, 0, 1) bottom row |
| `transform_point`, `transform_direction` | a point is divided by its w when w isn't exactly 1 (w = 0 gives (0, 0, 0)) |
| `look_at(eye, target, up)` | a rigid view matrix (world → camera) |
| `perspective(fov_y, aspect, near, far)` | tan(fov/2) = sin/cos of `sin_cos`; 90° gives a focal length of exactly 1; invalid values give the identity and assert |
| `orthographic(height, aspect, near, far)` | `height` world units visible vertically |
| `is_finite(Mat4)` | an integer test on the bits |

Pitfall: `normalize` is overloaded for `Vec3` and `Quat`, so a braced argument is ambiguous — write
`nnh::normalize(nnh::Vec3{1, 2, 3})`.

## Determinism and accuracy

- Exact: identities; rotations by the engine's quarter-turn constants (`kQuarterTurn`, `kHalfTurn`,
  `kThreeQuarterTurn`) and their products map axes to axes; a translation-only transform adds once per axis.
- Same bits on PC (every host compiler) and 3DS for inputs whose magnitudes are 0 or within 2⁻⁶⁰…2⁶⁰. Lengths, norms
  and determinants below 2⁻¹⁰⁰ are "zero" (an exponent test, so the 3DS's flush-to-zero never changes a result).
- Measured against `double` (tests/math3d/test_math3d_accuracy.cpp): unit quaternions 8.7·10⁻⁷, rotated directions
  1.4·10⁻⁶ (bound 2⁻¹⁸ = 3.8·10⁻⁶); points through chains of 8 transforms with coordinates up to 4096: 3.5·10⁻⁶ of the
  magnitude (bound 2⁻¹⁶); slerp 1.2·10⁻⁷.
- The models example's self-check runs 50 000 operations and must print the same checksum on PC and console
  (`3d: PASS A3BF4EDF`).

## Cost

A `Mat4` product is 64 multiplies and 48 adds; `to_mat4(Quat)` 9 divisions; `inverse` about 200 operations and 16
divisions. On an Old 3DS (VFPv2, ~15 cycles per division or square root) a transform's matrix costs a few
microseconds: build matrices once per object per frame, not per vertex (the GPU transforms vertices).
