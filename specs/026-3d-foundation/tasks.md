# Tasks 026: 3D foundation

Status: approved 2026-11-24 (overnight pre-approval)
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task: one or a few TDD loops. `[P]` = can be done in parallel with the previous task. Every stage ends with
`ctest --preset pc-debug` green, `scripts/dkp.sh cmake --build --preset 3ds-release` compiling and no new warning; work
may stop after any stage. Part **026a** = stages 0–10 (mono), part **026b** = stages 11–13 (stereo).

Prerequisite: spec 022 complete on `main`. Branch `spec-026` in a worktree from `main`.

## Stage 0 — baseline (026a)

- [x] **T0.1** Worktree `spec-026` from `main`; check `nnh::sin_cos`, `src/transform/sin_cos.cpp`, `src/core/det_sine.hpp`
  exist and their tests pass — files: none — test: `nnh_tests transform` — covers FR-3 (dependency)
- [x] **T0.2** Record baselines in plan 026 ("Stage 0 results"): `hello` sizes (3DS Release `.text`/`.3dsx`, PC Release
  `.exe`), 3DS memory report and VRAM of `hello`, clean engine build, edit-one-game-`.cpp` times (PC, 3DS), every
  example's screenshot hash (`NNH_EXIT_AFTER_FRAMES=30`) — test: script output saved under `build/baseline-026/` —
  covers FR-31, AC-9

## Stage 1 — 3D math (026a)

- [x] **T1.1** `<nnh/math3d.hpp>` skeleton, `Vec3/Vec4/Quat/Mat4/Transform3D/Ray3`, constexpr single-op helpers; CMake
  block `_nnh_math3d_sources` with `-ffp-contract=off` (+ `-fno-math-errno` for files using `sqrt`) — files:
  `math3d.hpp`, `engine/core/CMakeLists.txt` — test: `tests/math3d/test_vec3.cpp` (layout, defaults, trivially copyable)
  — covers FR-1
- [x] **T1.2** `dot`, `cross`, `length(_squared)`, `normalize` (2⁻¹⁰⁰ exponent test, assert), `math3d_internal.hpp` —
  files: `vec3.cpp` — test: `test_vec3.cpp`, `test_conventions.cpp` (x × y = z) — covers FR-1, FR-2
- [x] **T1.3** Quaternions: `quat_axis_angle`, `quat_euler`, product, `conjugate`, `normalize`, `to_mat4` by numerators
  over |q|², `rotate`; assert `sin_cos(kQuarterTurn/2)` has s == c bits (else the snap of plan P4) — files: `quat.cpp` —
  test: `test_quat.cpp`, `test_math3d_exact.cpp` (quarter/half/three-quarter turns about x, y, z map axes exactly) —
  covers FR-1, FR-3
- [x] **T1.4** `look_rotation` (Shepperd), `nlerp`, `slerp` (Eberly polynomial, shortest arc) — files: `quat_basis.cpp`,
  `quat_interp.cpp` — test: `test_quat_interp.cpp` (t = 0/1 exact, midpoints, opposite hemispheres) — covers FR-1, FR-3
- [x] **T1.5** `Mat4` product, `transpose`, `transform_point/direction`, `Mat4 × Vec4`, `inverse` (cofactors, det test),
  `inverse_affine`, `to_mat4(Transform3D)`, `is_finite` — files: `mat4.cpp`, `mat4_inverse.cpp`, `transform3d.cpp`,
  `finite3d.cpp` — test: `test_mat4.cpp`, `test_mat4_inverse.cpp`, `test_transform3d.cpp` (translation-only = one
  addition per axis) — covers FR-1, FR-3
- [x] **T1.6** `look_at`, `perspective`, `orthographic` (tan = s/c) — files: `projection.cpp` — test:
  `test_projection.cpp` (−z forward, near → −1, far → +1), `test_math3d_accuracy.cpp` (seeded inputs vs `double`: FR-3's
  bounds; write the measured maxima into plan 026 "Stage 1 results") — covers FR-1, FR-2, FR-3, AC-3
- [x] **T1.7** Golden script part 1 (math ops only) in `tests/gfx3d/golden_ops.cpp` (engine integer hash, no `rand`),
  the checkpoints every 10 000 — test: `test_golden_3d.cpp` (`math` golden recorded) — covers AC-2

## Stage 2 — meshes from code (026a)

- [x] **T2.1** `mesh/mesh_format.hpp` (header 112 B, submesh 48 B, texture 64 B, static_asserts), `mesh_layout.cpp`
  (strides 12/16/20, offsets) — test: `tests/mesh/test_mesh_layout.cpp` — covers FR-4
- [x] **T2.2** `mesh_quantize.cpp`, `mesh_bounds.cpp`, `flat_normals.cpp` — test: `test_mesh_quantize.cpp` (round trips,
  error ≤ half step, degenerate extents), `test_flat_normals.cpp` — covers FR-4, FR-5
- [x] **T2.3** `mesh_build.cpp` (`MeshDesc` → bytes + CRC; limits; refusals with reasons) — test: `test_mesh_build.cpp`,
  `test_mesh_limits.cpp` — covers FR-4, FR-5
- [x] **T2.4** `mesh_validate.cpp` + mini-fuzz case in `tests/fuzz/` + `libfuzzer_mesh_reader.cpp` — test:
  `test_mesh_validate.cpp` (every corruption: magic, version, sizes, offsets, index out of range, NaN, bad enum, path,
  CRC) — covers FR-7
- [x] **T2.5** Contract `video3d.hpp` (mesh part) + fake `video3d_fake.cpp` (`mesh_create/destroy` recorded) + pc/3ds
  stubs returning false (`"3D not built yet"`) — test: `tests/platform_conformance/test_video3d.cpp` (fake) — covers FR-25
- [x] **T2.6** `mesh.hpp`, `mesh_table.*` (256 slots, generations), `Mesh` handle (move, reset, deferred free),
  `create_mesh`, `texture_id` — files: `mesh.cpp`, `create_mesh.cpp`, `mesh_internal.hpp` — test:
  `test_create_mesh.cpp` (accounting `Linear`/`meshes`, `Unsupported` with 3D off) — covers FR-5, FR-8
- [x] **T2.7** [P] `primitives.cpp` (`make_cube`, `make_plane`, `make_sphere` with `sin_cos`) — test:
  `test_primitives.cpp` (vertex/triangle counts, normals unit within 1/127, bounds, uv ranges, byte hashes) — covers FR-5

## Stage 3 — glTF converter (026a)

- [x] **T3.1** `tests/data/gltf/make_fixtures.py` → `cube_textured.gltf/.bin/.png`, `multi_node.gltf` (baked
  transforms, a mirrored node), `vertex_colors.gltf`, `no_normals.gltf`, `cutout_blend.gltf`, `cube.glb`, and one
  `bad_<rule>.gltf` (+ `.txt` expected message) per refusal of FR-6/FR-7 — covers AC-4
- [x] **T3.2** `gltf_glb.cpp`, `gltf_document.cpp` (limits, required extensions) — test: `test_gltf_document.cpp` —
  covers FR-6, FR-7
- [x] **T3.3** `gltf_accessors.cpp` (types, normalised, strides, ranges, no sparse) — test: `test_gltf_accessors.cpp` —
  covers FR-6, FR-7
- [x] **T3.4** `gltf_nodes.cpp` (default scene, depth, TRS/matrix bake with math3d, mirrored winding, inverse-transpose
  normals) — test: `test_gltf_nodes.cpp` — covers FR-6
- [x] **T3.5** `gltf_primitives.cpp`, `gltf_materials.cpp` (paths, PNG rules via `png_size`, samplers, alpha, unlit),
  `gltf_merge.cpp` — test: `test_gltf_materials.cpp`, `test_gltf_merge.cpp` — covers FR-6
- [x] **T3.6** `cli_mesh.cpp` (`nnh-assetc mesh`, notes, `file: reason (at $.path)`, depfile with `.bin` and PNGs);
  shared core files and gltf sources with `-ffp-contract=off` in `tools/assetc/CMakeLists.txt` — test:
  `test_cli_mesh.cpp` (every fixture and every `bad_*`), **same bytes** as `create_mesh` from the converter's arrays —
  covers FR-6, AC-4
- [x] **T3.7** `libfuzzer_assetc_gltf.cpp` + `fuzz_assetc_gltf.cpp` (mini-fuzz in `nnh_tests`) — covers FR-7, AC-4
- [x] **T3.8** `cmake/nnh_assets.cmake`: `.gltf/.glb` step before the generic rules, P25's `.bin` not packed, `gltf`,
  `glb`, `nnhmesh` refused as data keys; `load_mesh.cpp` (validate, upload, textures) — test: `test_load_mesh.cpp`
  (fake assets), `ninja -d explain` after touching one model reconverts only it (PC and 3DS; note in plan) — covers FR-6,
  FR-8, AC-4

## Stage 4 — recording and the 3D pass on the fake platform (026a)

- [x] **T4.1** Rebase on `main`. `GfxConfig::max_draws_3d` (last member), `gfx/gfx3d_hook.*`, `gfx3d_ops.cpp`
  (registrar + link anchor used by `record.cpp` and `mesh_table.cpp`), `gfx3d_init.cpp` (records, spheres, order,
  keys; contract `video3d_init`) — test: `tests/gfx3d/test_init3d.cpp` (0 → nothing reserved; capacity → bytes as plan)
  — covers FR-26, FR-31
- [x] **T4.2** `<nnh/gfx3d.hpp>` types; `record.cpp` + `material_check.cpp` (finite bits, bottom row, submesh, stale
  mesh, enums, shininess, stale texture → untextured + assert; Release clamps); world sphere at record — test:
  `test_record.cpp`, `test_material.cpp`, `test_misuse3d.cpp` — covers FR-9, FR-17, FR-29
- [x] **T4.3** `camera3d.cpp` (`set_camera` validation, `point_at`), `lighting.cpp` (defaults, ≤ 4, world → view),
  `light_tables.cpp` (D0 levels, distance table) — test: `test_camera3d.cpp`, `test_lighting.cpp`,
  `test_light_tables.cpp` — covers FR-11, FR-12
- [x] **T4.4** `frustum.cpp`, `cull.cpp` (sphere vs planes; counted) — test: `test_cull.cpp` (inside, outside,
  straddling, behind, ortho) — covers FR-20
- [x] **T4.5** `order.cpp` (opaque/cut-out in call order; blended by centre depth, stable merge sort, ties by call) —
  test: `test_order.cpp` — covers FR-19
- [x] **T4.6** `TileSplit::layer` in `draw_list`; `gfx/world_split.*`; `gfx_submit_slice` extracted in `gfx_frame.cpp`;
  hook calls — test: `tests/gfx/test_world_split.cpp`; every existing `tests/gfx` test unchanged; a 2D-only frame's
  platform call log byte-identical to before (`test_screen_pass.cpp`) — covers FR-18, AC-9
- [x] **T4.7** `submit3d.cpp`, `screen_pass.cpp` (split, cull, order, per draw MV with quantisation fold, cofactor
  normal matrix, uv decode, resolved material; `begin_3d/draw_3d/end_3d`; `below_2d`), `eye_setup.cpp` (mono) — test:
  `test_screen_pass.cpp` (2D below → 3D → 2D above → debug last; world layer 0 and 16), FR-25 log — covers FR-18,
  FR-19, FR-25
- [x] **T4.8** `screen_project.cpp` (`to_screen`, `screen_ray`) — test: `test_screen_project.cpp` (round trips, flags,
  ortho) — covers FR-13
- [x] **T4.9** `stats3d.cpp`, overlay line, `debug_draw3d.cpp` (Debug/Dev lines on layer 15) — test:
  `test_stats3d.cpp`, `test_debug_draw3d.cpp` — covers FR-28
- [x] **T4.10** Golden part 2: frustum tests, recorded frame orders, `to_screen`/`screen_ray` added → final `3d` golden
  (50 000 ops) — test: `test_golden_3d.cpp` — covers AC-2, FR-21
- [x] **T4.11** [P] `test_no_alloc.cpp` (100 frames of 256 draws, zero allocations); grep check "no `publish`" — covers
  FR-27, FR-30

## Stage 5 — CPU rasteriser (026a)

- [ ] **T5.1** `engine/platforms/shared/raster3d/` library (`-O2 -ffp-contract=off` all flavours), `raster3d.hpp`,
  `vertex_stage.cpp` — test: `tests/raster3d/test_vertex_stage.cpp` — covers FR-22
- [ ] **T5.2** `clip.cpp` (near/far + guard band) — test: `test_clip.cpp` (a triangle through the near plane clipped,
  not dropped) — covers FR-20, FR-22
- [ ] **T5.3** `setup.cpp`, `raster.cpp` (28.4 edges, top-left rule, back-face cull, depth) — test: `test_coverage.cpp`
  (shared edge: no gap, no double), `test_depth.cpp` (intersecting triangles) — covers FR-22, AC-5
- [ ] **T5.4** `sample.cpp` (nearest, bilinear, repeat/clamp/mirror), perspective-correct attributes — test:
  `test_sample.cpp`, `test_perspective.cpp` (checkerboard on a tilted plane) — covers FR-22, AC-5
- [ ] **T5.5** `shade.cpp`, `lut.cpp` (shared tables), `blend.cpp` (cut-out, blend) — test: `test_shade.cpp` (lit pixels
  = FR-10 ±1), `test_blend.cpp` — covers FR-10, FR-22, AC-5
- [ ] **T5.6** Fake platform pixels (`fake::enable_pixels_3d`, `pixel_3d`, RGBA copies of fake textures) — test:
  `tests/gfx3d/test_pixels_fake.cpp` (an end-to-end frame through core) — covers FR-25

## Stage 6 — PC backend (026a)

- [ ] **T6.1** Rebase on `main`. `video3d_pc.cpp`, `mesh3d_pc.cpp` — test: conformance `test_video3d.cpp` with the pc
  platform (headless) — covers FR-22
- [ ] **T6.2** `texels3d_pc.cpp` (read-back cache by slot + generation) — test: conformance (a texture's texels equal
  its PNG's) — covers FR-22
- [ ] **T6.3** `pass3d_pc.cpp` (read-back when `below_2d`, rasterise, opaque upload) — test: a temporary example frame,
  screenshot read by hand — covers FR-18, FR-22
- [ ] **T6.4** Golden PNG hash check script `scripts/check_png_hash.sh`; identical hashes with GPU renderer,
  `NNH_SIM_RENDERER=sdl` and `SDL_VIDEO_DRIVER=offscreen` on Windows — covers FR-23, AC-5
- [ ] **T6.5** Measure pc-dev pass time (read-back, raster, upload); decide P15's fallback if read-back > 2 ms (note in
  plan) — covers AC-8 (simulator)

## Stage 7 — 3DS backend, mono (026a)

- [ ] **T7.1** `shaders/mesh3d.v.pica`; CMake `ctr_add_shader_library` + `dkp_add_embedded_binary_library`; clean 3DS
  build time measured — covers FR-31
- [ ] **T7.2** `video3d_3ds.cpp` (left target D24S8, program, attribute layouts 12/16/20 with fixed attributes,
  depth off after `C2D_Prepare`), `app_3ds.cpp` command buffer, `mesh3d_3ds.cpp` — covers FR-26
- [ ] **T7.3** `lights3d_3ds.cpp` (light env, LUTs from `light_tables.cpp`, distance attenuation) — covers FR-10, FR-11
- [ ] **T7.4** `pass3d_3ds.cpp`, `tilt3d_3ds.cpp` (core projection × tilt/depth matrix; a Development self-test logs
  `3d tilt: PASS` when it matches `Mtx_PerspTilt` within 1e-5) — covers FR-10, FR-19, FR-22 (console counterpart)
- [ ] **T7.5** `probe.cpp` (core, Dev only) + read-back through `remote_capture` — test: `test_probe.cpp` on fake —
  covers FR-24
- [ ] **T7.6** Console smoke (maintainer presses Y): temporary `MODELS_DEMO=smoke` draws a flat cube, then a lit
  textured one; `nnh run models --script examples/models/tests/smoke.nnhs` expects `3d tilt: PASS` and takes a
  screenshot; compare with the PC's by reading both — covers FR-22 (console)

## Stage 8 — example (026a)

- [ ] **T8.1** `examples/models/art/make_models.py` → `assets/models/ship.gltf/.bin`, `ship.png`, `fence.png`,
  `checker.png`; `examples/models/CMakeLists.txt` (`ASSETS`, `max_draws_3d = 600`) — covers AC-6
- [ ] **T8.2** Showcase scene (mono) + HUD + name tag + bottom-screen buttons — screenshots left eye, orthographic,
  debug draw (`NNH_EXIT_AFTER_FRAMES`), read; golden hashes recorded in `examples/models/tests/golden.txt` — covers AC-5,
  AC-6
- [ ] **T8.3** `self_check.*` (AC-2's script, shared with `tests/models_example/test_self_check.cpp`) + `probe_scenes.*`
  + `shading_ref.*` (flat, textured quad, lit sphere, cut-out, blend over depth, 2D over 3D and behind) → `3d: PASS`,
  `3d pixels: PASS` on PC — covers AC-2, AC-7
- [ ] **T8.4** Bench scene (mono cases of AC-8) → `3d bench: PASS <numbers>` lines — covers AC-8
- [ ] **T8.5** `tests/check.nnhs` and `tests/bench.nnhs` (below), passing with `--pc` — covers AC-7

## Stage 9 — mutation, Linux VM, opt-in (026a)

- [ ] **T9.1** `tests/models_example/` and full suite on Windows; `nnh_tests` timing — covers AC-1
- [ ] **T9.2** Hand mutations (plan "Test strategy") each caught; note in plan — covers AC-1
- [ ] **T9.3** VM 120 (start only if Bazzite is stopped; `free -m`): GCC 13/14 and clang 18 builds + ctest, ASan/UBSan,
  goldens equal to Windows (`3d` golden, PNG hashes with `SDL_VIDEO_DRIVER=offscreen` and `NNH_SIM_RENDERER=sdl`),
  libFuzzer gltf and mesh reader 10 min each clean; shut the VM down — covers AC-1, AC-4, AC-5
- [ ] **T9.4** AC-9: `hello` sizes vs T0.2 (budget P14), `scripts/check_no_3d_symbols.sh` (3DS `.map`, PC `nm`), every
  example's screenshot hash unchanged, every earlier script passing on PC, 2D-only VRAM report unchanged, build times —
  covers AC-9, FR-31

## Stage 10 — console run and docs (026a)

- [ ] **T10.1** Console run (fully prepared below), mono — covers AC-7, AC-8, FR-24
- [ ] **T10.2** Record results in spec/plan/docs (tolerances confirmed or set; bench numbers; targets per policy) — covers
  AC-8, FR-24
- [ ] **T10.3** `docs/api/math3d.md`, `docs/api/mesh.md`, `docs/api/gfx3d.md` (+ header comments), guide
  `docs/guides/models.md` (Blender glTF export: +Y up, apply transforms, PNG beside, `.bin` naming, what is ignored);
  links from `math.md`, `gfx.md`, `texture.md`, `assets.md` — covers AC-10
- [ ] **T10.4** `docs/architecture/rendering.md` (the 3D pass, world layer, hook, PC rasteriser — replaces "the same
  `SDL_GPUDevice`" and "3D will need a GPU device"), `docs/tech/sdl3.md` ("Renderer: 2D now, 3D later" → the CPU
  rasteriser, D41), `docs/tech/devkitpro-3ds.md` (targets, shader, LUTs, VRAM, costs), `project-structure.md`
  (`shared/raster3d`, `src/math3d`, `mesh`, `gfx3d`), D41–D44 change-log "implemented (026a)", `docs/REFERENCES.md`,
  CLAUDE.md layout — covers AC-10

## Stage 11 — slider everywhere (026b)

- [ ] **T11.1** Rebase. Contract `stereo_slider()`; `system::stereo_slider()` (+ override); fake `set_stereo_slider`;
  `slider_3ds.cpp`; PC `Stereo3D` capability — test: `tests/system/test_stereo_slider.cpp` — covers FR-16
- [ ] **T11.2** PC keys F6/F7/F8, `NNH_SIM_SLIDER`, `NNH_SIM_EYE`, title notice, `window.ini` (not for scripted runs;
  `NNH_REMOTE` forces the left view) — test: `tests/platform_pc/test_sim_settings.cpp` (if present) + screenshots —
  covers FR-16, FR-22
- [ ] **T11.3** Remote `Slider` message (type 7) in `remote_wire`, session, `remote_slider.cpp` — test:
  `test_remote_wire.cpp`, `test_remote_session.cpp` — covers FR-16
- [ ] **T11.4** `tools/nnh`: `slider V|off`, `screenshot NAME eyes` (screen 2 → `-top-right.png`), runner counts screens
  from the mask; `remote_capture_right` (pc, 3ds, fake) — test: `tools/nnh/tests/test_script_parser.cpp`,
  `test_script_runner.cpp` — covers FR-16, AC-7

## Stage 12 — stereo eyes (026b)

- [ ] **T12.1** `eye_setup.cpp` stereo: eyes = 2 when capability, slider > 0, `stereo.enabled`, perspective, frame has
  3D; off-axis frusta (`shift = sep/2 × near/focal`), eye offset along camera right — test: `test_eyes.cpp` (focal-plane
  point projects to the same pixel in both eyes; nearer/farther signs; slider 0 / ortho / off → 1 eye) — covers FR-14
- [ ] **T12.2** Culling union, per-eye passes, stats per eye; `select_eye`/`set_stereo` calls; debug pass per eye
  (`debug_pass.cpp`) — test: `test_screen_pass_stereo.cpp` (identical 2D lists per eye, debug last per eye) — covers
  FR-14, FR-15, FR-20
- [ ] **T12.3** Stereo golden `3d-stereo` (per-eye matrices, union decisions) — test: `test_golden_3d.cpp` — covers AC-2
- [ ] **T12.4** PC `eyes_pc.cpp` (right target, eye views, anaglyph, restore after the frame) — screenshots right eye
  (`NNH_SIM_SLIDER=1 NNH_SIM_EYE=right`) and anaglyph read; right-eye golden hash — covers FR-22, FR-23, AC-5
- [ ] **T12.5** 3DS `eyes_3ds.cpp` (right D24S8 target, `targets[0]` swap, `C2D_TargetClear` at first use, `gfxSet3D` on
  change), `remote_capture_right_3ds.cpp` — covers FR-14, FR-26

## Stage 13 — stereo example, console, docs, status (026b)

- [ ] **T13.1** Showcase: slider value on the bottom screen; stereo probe scene (both eyes) in check mode; stereo bench
  cases — covers AC-6, AC-7, AC-8
- [ ] **T13.2** `tests/stereo.nnhs` (below), passing with `--pc` — covers AC-7
- [ ] **T13.3** VM re-run (stereo golden, PNG hashes, ASan) — covers AC-1, AC-5
- [ ] **T13.4** AC-9 re-measured (PC keys inside P14's budget) — covers AC-9
- [ ] **T13.5** Console run stereo (prepared below) — covers AC-7, AC-8
- [ ] **T13.6** Stereo defaults judged on the Old 3DS XL (maintainer looks at the showcase at slider 1/2 and 1: yes/no
  "comfortable"); adjust P12's ratio if no — covers FR-14
- [ ] **T13.7** Docs: stereo sections in `gfx3d.md`, `simulator.md` (F6/F7/F8, env vars), `3ds-extensions.md` (portable
  slider beside `slider_3d()`), `system.md`, `guides/testing-on-the-console.md` (`slider`, `screenshot NAME eyes`),
  `nnh-tool.md` — covers AC-10
- [ ] **T13.8** `docs/ROADMAP.md` M8 first part checked; README status — covers AC-10
- [ ] **T13.9** spec, plan, tasks status → implemented; `specs/README.md` row; D41–D44 change log "implemented" —
  covers all

## Console runs (fully prepared)

Before each run: the PC and the console on the LAN (console IP 192.168.10.52), the Homebrew Launcher open; the
maintainer presses **Y** when asked and nothing else. Logs and PNGs land in `build/logs/`. Expected values marked
`<…>` are copied from the PC run of the same script (pc-debug) the moment T8.5/T13.2 pass; the console must print the
**same** math lines.

### T7.6 smoke (026a)

```sh
build/pc-debug/tools/nnh/nnh run models --pc --script examples/models/tests/smoke.nnhs   # PC first: exit 0
build/pc-debug/tools/nnh/nnh run models --script examples/models/tests/smoke.nnhs        # console (Y)
```

`smoke.nnhs`: `fail-on "] E "` · `wait 30` · `expect "3d tilt: PASS" 120` · `wait 3` · `screenshot flat` · `tap A` ·
`expect "smoke: lit" 30` · `wait 10` · `screenshot lit` · `end`. Expected: `nnh: PASS`, exit 0; then read
`build/logs/<run>/flat-top.png` and `lit-top.png` beside the PC's.

### T10.1 check and bench, mono (026a)

```sh
build/pc-debug/tools/nnh/nnh run models --pc --script examples/models/tests/check.nnhs
build/pc-debug/tools/nnh/nnh run models --script examples/models/tests/check.nnhs        # console (Y)
build/pc-debug/tools/nnh/nnh run models --script examples/models/tests/bench.nnhs        # console (Y)
```

`examples/models/tests/check.nnhs` (026a part):

```
fail-on "] E "
fail-on "3d: FAIL"
fail-on "3d pixels: FAIL"
wait 30
screenshot showcase
# check mode (SELECT): the 50 000-operation script, then the probe scenes
tap SELECT
expect "models: check started" 20
expect "3d: 10000 sum <c1>" 120
expect "3d: 20000 sum <c2>" 120
expect "3d: 30000 sum <c3>" 120
expect "3d: 40000 sum <c4>" 120
expect "3d: 50000 sum <c5>" 120
expect "3d: PASS <c5>" 30
expect "3d pixels: PASS" 300
wait 5
screenshot probes
# back to the showcase: fixed inputs and to_screen answers
tap SELECT
expect "models: showcase" 20
wait 3
stick circle 1 0
wait 30
stick circle 0 0
expect "tag: <x1>,<y1> depth <d1>" 10
wait 3
tap X
expect "projection: orthographic" 10
wait 3
expect "tag: <x2>,<y2> depth <d2>" 10
wait 10
screenshot ortho
tap X
wait 3
touch 40 200
expect "debug draw: on" 10
wait 3
untouch
wait 10
screenshot debug
end
```

Expected on the console: every line above, `nnh: PASS`, exit 0; PNGs `showcase-top.png`, `probes-top.png`,
`ortho-top.png`, `debug-top.png` (+ bottoms) compared with the PC's by reading them (same geometry; colours close).

`examples/models/tests/bench.nnhs`: `fail-on "] E "` · `fail-on "3d bench: FAIL"` · `wait 30` · `tap Y` ·
`expect "3d bench: started" 20` · `expect "3d bench: throughput 12000 avg" 700` · `expect "3d bench: draws 256 avg" 700`
· `expect "3d bench: cpu 512 avg" 700` · `expect "3d bench: fill avg" 700` · `expect "3d bench: PASS" 30` · `wait 10` ·
`screenshot bench` · `end`. Each case line: `3d bench: <case> avg <ms> worst <ms> missed <n>/600 cpu <ms>`. Then the
2D gate: `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/gate.nnhs` → `bench: PASS`.

Pass criteria: the math lines equal the PC's bit for bit; `3d pixels: PASS` (FR-24's tolerance; a FAIL line names
scene, pixel, got, want — then record the measured difference and apply the spec's policy); bench numbers recorded
in plan 026 ("Stage 10 results") whatever the verdict.

### T13.5 stereo (026b)

```sh
build/pc-debug/tools/nnh/nnh run models --pc --script examples/models/tests/stereo.nnhs
build/pc-debug/tools/nnh/nnh run models --script examples/models/tests/stereo.nnhs       # console (Y)
build/pc-debug/tools/nnh/nnh run models --script examples/models/tests/bench.nnhs        # with "slider 1" cases
```

`stereo.nnhs`:

```
fail-on "] E "
fail-on "3d pixels: FAIL"
wait 30
slider 1
wait 3
expect "slider: 1.00" 10
screenshot stereo eyes
tap SELECT
expect "3d: PASS <c5>" 900
expect "3d-stereo: PASS <s5>" 300
expect "3d pixels: PASS" 300
expect "3d pixels stereo: PASS" 300
tap SELECT
wait 3
slider 0
expect "slider: 0.00" 10
expect "eyes: 1" 10
wait 3
slider off
end
```

Expected: `stereo-top.png`, `stereo-top-right.png` differ horizontally only (read both), `nnh: PASS`. The bench adds
`3d bench: throughput 6000 stereo avg …` and `3d bench: draws 256 stereo avg …`. T13.6: the maintainer moves the
slider by hand once, answers "comfortable? yes/no".
