# mesh — meshes and materials for 3D

Header: `<nnh/mesh.hpp>` · Spec: [026](../../specs/026-3d-foundation/spec.md) ·
Plan: [026](../../specs/026-3d-foundation/plan.md) · Decision: [D42](../architecture/decisions.md) ·
Drawing: [gfx3d.md](gfx3d.md) · Math: [math3d.md](math3d.md) · Export guide: [models.md](../guides/models.md)

A **mesh** is 1..32 submeshes of triangles (16-bit indices into at most 65 535 vertices), each with a default
**material**. Make one from your own arrays (`create_mesh`), use a primitive (`make_cube`, `make_plane`, `make_sphere`)
or load a model converted from glTF 2.0 at build time (`load_mesh`). Meshes need 3D on
(`AppConfig::gfx.max_draws_3d > 0`): otherwise every call fails with `Unsupported`.

```cpp
auto ship = nnh::load_mesh("models/ship");                  // assets/models/ship.gltf, converted by the build
auto ball = nnh::make_sphere(0.5f, 12, 24, {.base_color = nnh::colors::red, .specular = nnh::colors::white});
if (!ship || !ball) return false;
```

## Making meshes

| Function | Does |
|----------|------|
| `create_mesh(const MeshDesc&)` | positions (required), normals (null → flat normals, vertices unwelded per triangle), uvs, colours, indices (null → 0, 1, 2, …), submeshes (null → one); front faces counter-clockwise |
| `make_cube(size, material)` | 24 vertices, 12 triangles, texture coordinates 0..1 per face |
| `make_plane(width, depth, subdivisions, material)` | x/z plane facing +y, `subdivisions` (1..64) cells per side |
| `make_sphere(radius, rings, segments, material)` | rings 2..64, segments 3..128 |
| `load_mesh("models/ship")` | `<path>.nnhmesh` from the assets, validated (never trusted); its textures are loaded and owned by the mesh |
| `texture_id(texture)` | the `TextureId` a material names a texture by |

All of them allocate (load time only) and fail with a `Result` error: `InvalidArgument` (with the reason, also logged),
`Unsupported` (3D off), `OutOfMemory`, `LimitReached` (more than 256 meshes), `NotFound`, `Corrupted`.

`create_mesh` uses the same builder as `nnh-assetc mesh`: the same arrays give **the same bytes** either way.

`Mesh` is a move-only RAII handle: `valid()`, `submesh_count()`, `vertex_count()`, `triangle_count()`, `bounds()`,
`bounding_sphere()`, `material(submesh)`, `reset()`. Destroying one inside a frame frees it once the GPU has drawn
that frame (during the next `nnh::frame()`; draws recorded with it still draw, as with 2D textures; D47); after
`nnh::shutdown()` handles are stale.

## Vertex format and memory

Vertices are **quantised** (the `.nnhmesh` format, D42): position int16 ×4 over the mesh's box, normal int8 ×4,
texture coordinates int16 ×2 (when the mesh has them), colour u8 ×4 (when it has them): **12, 16 or 20 bytes**. Errors:
positions within half a step (extent / 65 534: 15 µm on a 1 m model, 7.8 mm on a 512-unit level — split large levels
into several meshes), normals within 0.5°, texture coordinates within range / 65 534. The GPU decodes them for free
(the decode is folded into the model-view matrix).

Vertex and index data live in the console's linear memory and are accounted as `Linear` / `meshes` on every system
(the PC counts the bytes the console would use); the mesh's record and its submeshes are a small heap block (`meshes`).
Example: 6 100 vertices × 20 B + 36 000 indices × 2 B ≈ 194 KB.

## Materials

`Material3D` is a plain value (28 bytes):

| Field | Default | Meaning |
|-------|---------|---------|
| `base_color` | white | multiplies everything |
| `specular` | black | highlight colour (black = none) |
| `texture` | `{}` | a `TextureId` (untextured when empty; a destroyed texture draws untextured and asserts) |
| `shininess` | 3 | level 0..7 = exponent 2^(level+1): 2, 4, 8, 16, 32, 64, 128, 256 |
| `alpha` | `Opaque` | `Cutout` (pixels below `alpha_cutoff` discarded) or `Blend` (mixed, after opaque draws, back to front) |
| `alpha_cutoff` | 128 | 0..255 |
| `filter` | `Nearest` | or `Linear` |
| `wrap_u`, `wrap_v` | `Repeat` | `Clamp`, `Mirror` |
| `lit` | true | false: the surface colour as is |
| `vertex_colors` | true | multiply the mesh's vertex colours (white when it has none) |
| `cull_back` | true | skip triangles facing away |

The shading formula and how a draw overrides a material: [gfx3d.md](gfx3d.md#materials-and-the-shading-formula).

## Converted models

`*.gltf` / `*.glb` files in the game's assets folder are converted by the build into `<name>.nnhmesh` (one step per
model, rerun when the model, its buffers or its textures change). Buffers named `<model>.bin` or `<model>_<any>.bin`
beside a `.gltf` are not packed. A texture must be a PNG inside the assets folder (power-of-two sides from 8 to 1024)
and becomes an ordinary texture owned by the mesh (at most 8 per model). What is read, ignored and refused, and how to
export from Blender: [models.md](../guides/models.md).

## Limits

| Limit | Value |
|-------|-------|
| vertices / indices per mesh | 65 535 / 786 432 |
| submeshes / textures per mesh | 32 / 8 |
| meshes loaded at once | 256 |
| coordinates | \|x\| ≤ 2²⁰ |

## Hot reload (spec 031)

In a Debug/Development game under `nnh run`, a mesh loaded with `load_mesh` reloads when its model is saved (the build
reconverts the glTF into the same `.nnhmesh`): the same `Mesh` handle shows the new model — another vertex count, other
submeshes and textures — from the frame the batch is applied; a damaged file refuses the whole batch and the old model
stays. Meshes made with `create_mesh` don't reload. See [assets.md](assets.md#hot-reload).
