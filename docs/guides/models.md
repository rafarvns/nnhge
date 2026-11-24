# Exporting 3D models

How to get a model from Blender (or any tool that writes glTF 2.0) into an NNHGE game. Reference: [mesh.md](../api/mesh.md),
[gfx3d.md](../api/gfx3d.md); format decision [D42](../architecture/decisions.md); spec [026](../../specs/026-3d-foundation/spec.md).

## In short

1. Model with +Y up (Blender's export converts its +Z up for you: keep **+Y Up** checked).
2. Apply transforms you don't want baked twice (Object → Apply → All Transforms is the safest).
3. Use one image texture per material, a **PNG** with power-of-two sides from 8 to 1024, and save it **inside the game's
   assets folder** (beside the model or in a folder like `art/`).
4. File → Export → glTF 2.0: format **glTF Separate (.gltf + .bin + textures)** or **glTF Binary (.glb)** without
   embedded images; Include: selected objects or the scene; Transform: +Y Up; Geometry: UVs, Normals, Vertex Colors
   as needed; turn **Compression (Draco) off**.
5. Put the `.gltf` and its `.bin` (named `<model>.bin` or `<model>_<anything>.bin`, Blender's default) in the assets
   folder, e.g. `assets/models/ship.gltf`. The build converts it to `models/ship.nnhmesh`; load it with
   `nnh::load_mesh("models/ship")`.

Editing the model, its `.bin` or its PNG reconverts only that model.

## What is read

The default scene's nodes, their transforms **baked** into one mesh (mirrored nodes keep their faces outward);
triangle primitives (indexed or not); `POSITION`, `NORMAL` (flat normals are made when missing), `TEXCOORD_0`,
`COLOR_0`; each material's `baseColorFactor`, `baseColorTexture`, `alphaMode` (`OPAQUE`, `MASK` → cut-out, `BLEND`),
`alphaCutoff`, `doubleSided` (→ no back-face culling), `KHR_materials_unlit` (→ unlit), and the sampler's filter (no
sampler: linear) and wrap modes. Primitives sharing a material become one submesh.

## What is ignored (the model still builds; the build prints a note)

Skins, animations, morph targets, cameras, lights (`KHR_lights_punctual`), extra texture coordinate sets,
metallic/roughness, normal, occlusion and emissive maps, unknown extensions that aren't required. A rigged character
exports as its rest pose.

## What is refused (`file: reason (at $.json.path)`)

Non-triangle primitives (points, lines, strips), sparse accessors, required extensions it doesn't know (Draco, meshopt,
texture transforms), images inside the file or as data URIs, buffers not named after the model, textures that aren't
PNGs inside the assets folder or break the size rule, more than 65 535 vertices or 32 materials or 8 textures after
merging, malformed or out-of-range data. Split a large level into several models.

## Materials beyond glTF

glTF has no Blinn-Phong specular: converted materials have no highlight. Give one in code with a material override
(`DrawOptions3D::material`, see [gfx3d.md](../api/gfx3d.md)) or build the mesh with `create_mesh` and your own
`Material3D`.
