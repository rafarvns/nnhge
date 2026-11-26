# References and prior art

Projects to study (learn from them; check licenses before reusing any code). Researched 2026-11-17.

## Toolchain and low-level libraries (we build on these)
- **devkitPro / devkitARM, libctru, citro3d, citro2d, 3dstools, tex3ds** — the 3DS homebrew toolchain.
  Examples: https://github.com/devkitPro/3ds-examples
- **3DS Homebrew Launcher (hbmenu)** — netloader (press Y) receives `.3dsx` from `3dslink`:
  https://github.com/devkitPro/3ds-hbmenu
- **libctru `3ds/3dslink.h`** — `link3dsStdio()` redirects stdout/stderr to the PC (TCP port 17491):
  https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/3dslink.h
- **makerom** v0.19.0 (3DSGuy/Project_CTR, MIT) and **bannertool** 1.2.3 (carstene1ns/3ds-bannertool, the maintained fork, MIT) — build `.cia`; not bundled with devkitPro, fetched pinned by our build (spec 009): https://github.com/3DSGuy/Project_CTR · https://github.com/carstene1ns/3ds-bannertool
  · https://www.3dbrew.org/wiki/Makerom · Makefile pattern: https://github.com/TricksterGuy/3ds-template
- **FBI** (Steveice10) — installs `.cia` files; its "Receive URLs over the network" protocol (used by
  `nnh install`) is the one of its `servefiles.py`. The RSF of **ftpd** (mtheall) was the
  reference for system call numbers and service names in our `nnhge.rsf`: https://github.com/mtheall/ftpd
- **3dbrew wiki** — hardware and system reference: https://www.3dbrew.org/wiki/Setting_up_Development_Environment
- **stb_vorbis** (Sean Barrett; public domain or MIT) — the Ogg Vorbis decoder we vendor in
  `third_party/stb_vorbis` (spec 008; version and hash in its README): https://github.com/nothings/stb
- **libsndfile / python-soundfile** — used only by `make_sounds.py` scripts to encode the test and example
  sounds (not shipped): https://python-soundfile.readthedocs.io
- **Tiled** (Thorbjørn Lindeijer; GPL editor, its file formats are documented and free to read) — the map editor
  whose JSON map (`.tmj`) and tileset (`.tsj`) formats `nnh-assetc` converts (spec 012, read with our own JSON
  reader; nothing of Tiled is shipped or needed to build). Format reference (Tiled 1.10, also 1.9's `class`):
  https://doc.mapeditor.org/en/stable/reference/json-map-format/ · flip bits in gids:
  https://doc.mapeditor.org/en/stable/reference/global-tile-ids/ · parallax (centre-based scrolling, parallax
  origin): https://doc.mapeditor.org/en/stable/manual/layers/#parallax-scrolling-factor
- **Aseprite** (Igara Studio; paid editor, MIT-licensed sources read for the export format only) — its sprite sheet
  JSON (`frames` Hash/Array, `meta.frameTags` with `direction` and `repeat` written as a string, `meta.slices` with
  pivots) is what `nnh-assetc sprite` converts (spec 016, our own reader; nothing of Aseprite is shipped or needed to
  build): https://www.aseprite.org/docs/sprite-sheet/ · https://www.aseprite.org/docs/cli/ ·
  https://www.aseprite.org/docs/slices/ · ping-pong and repeat semantics checked against `src/doc/playback.cpp` and
  `src/app/doc_exporter.cpp` (github.com/aseprite/aseprite)

## Engines and frameworks (design inspiration)
- **Butano** (GBA, modern C++, zlib) — best model for API style and documentation quality; no exceptions/RTTI,
  one-line sprites/backgrounds, great examples. https://github.com/GValiente/butano · https://gvaliente.github.io/butano/
- **LÖVE Potion** — LÖVE (Lua) on 3DS/Switch/Wii U; shows how to keep one API across consoles and its
  compatibility tradeoffs. https://github.com/lovebrew/lovepotion · https://lovebrew.github.io/
- **cpp3ds** — SFML-like C++ framework for 3DS with a desktop "emulator" build; closest to our simulator
  idea (unfinished). https://github.com/cpp3ds/cpp3ds
- **quicklime** — 3D 3DS engine in C++ using EnTT: https://github.com/syelan34/quicklime
- **Tridingine-3DSlib** — C++ library targeting PC and 3DS from one codebase (2026).
- **Godot 3DS port** (abandoned, Godot 2.1) — what a full engine port costs: https://www.gamebrew.org/wiki/Godot_Engine_3DS
- **SDL3** — official 3DS platform (`SDL_PLATFORM_3DS`); we use SDL3 on PC: https://wiki.libsdl.org/SDL3/SDL_PLATFORM_3DS
- **Godot Tween and Timer** — tweens bound to a node, start values captured when a tweener starts, chained and
  parallel steps, intervals and callbacks; our scene-bound tween sets follow them (spec 015):
  https://docs.godotengine.org/en/stable/classes/class_tween.html ·
  https://docs.godotengine.org/en/stable/classes/class_timer.html
- **Godot AnimatedSprite2D / SpriteFrames** — named animations with a speed and relative frame durations, shared
  frames, `animation_finished`/`animation_looped` signals; our sprite libraries, animators and notices follow them
  (spec 016): https://docs.godotengine.org/en/stable/classes/class_animatedsprite2d.html ·
  https://docs.godotengine.org/en/stable/classes/class_spriteframes.html
- **Unity Animator and animation events** — states → clips, transitions with exit time, Any State; events at a
  frame; our state machine's rule table (AtEnd = exit time, `any()` = Any State) and frame markers (spec 016, without
  Unity's parameters): https://docs.unity3d.com/Manual/class-Animator.html ·
  https://docs.unity3d.com/Manual/class-Transition.html ·
  https://docs.unity3d.com/Manual/script-AnimationWindowEvent.html
- **DOTween** (Unity) — getter/setter targets, `Sequence` with Append/Join/AppendInterval/AppendCallback,
  Restart/Yoyo loops, `SetLink` to tie a tween's life to an object: https://dotween.demigiant.com/documentation.php
- **Godot InputMap and actions** — named actions with several events, per-action dead zone,
  `is_action_just_pressed`/`get_axis`/`get_vector`; our input actions follow them (spec 017):
  https://docs.godotengine.org/en/stable/tutorials/inputs/inputevent.html ·
  https://docs.godotengine.org/en/stable/classes/class_inputmap.html
- **Unity Input System** — Button/Value actions, 2D-vector composites, action maps enabled and disabled, interactive
  rebinding with cancel and excluded controls, binding overrides saved as differences (spec 017):
  https://docs.unity3d.com/Packages/com.unity.inputsystem@1.11/manual/Actions.html ·
  https://docs.unity3d.com/Packages/com.unity.inputsystem@1.11/manual/ActionBindings.html#interactive-rebinding
- **Robert Penner's easing equations** (BSD licence) — the standard curves and their constants, which our 19
  easings re-derive without libm: http://robertpenner.com/easing/
- **Unity ScriptableObjects** — typed data assets defined by a class and edited in the Inspector; our data assets
  keep the idea (a type in code, many asset files, references between them) without an editor (spec 014):
  https://docs.unity3d.com/Manual/class-ScriptableObject.html
- **Godot Resources** — exported properties saved as text `.tres` in the repository and as binary `.res` in the
  game: the same text-in-repo, binary-in-game split as spec 014:
  https://docs.godotengine.org/en/stable/tutorials/scripting/resources.html
- **Godot Camera2D** — drag margins (our dead zone), position smoothing, `limit_*`, zoom, offset, editor drawing of
  margins and limits; our camera helper follows its ideas (spec 018; no code copied):
  https://docs.godotengine.org/en/stable/classes/class_camera2d.html
- **Unity Cinemachine** — the Framing Transposer's dead zone and damping, Confiner 2D, Impulse for shakes (spec 018):
  https://docs.unity3d.com/Packages/com.unity.cinemachine@2.10/manual/index.html
- **Squirrel Eiserloh, "Math for Game Programmers: Juicing Your Cameras With Math"** (GDC 2016) — trauma in [0, 1],
  shake = trauma², linear decay, smooth noise instead of random values, shake added to a copy of the camera; our
  trauma shake and seeded value noise are written from the talk (spec 018):
  http://www.mathforgameprogrammers.com/gdc2016/GDC2016_Eiserloh_Squirrel_JuicingYourCameras.pdf
- **Godot physics layers and queries** — 32 layers, "layer = what I am, mask = what I scan", one-way detection; our
  `collision::Layers`, `detects` and `QueryFilter` follow it (spec 019; no code copied):
  https://docs.godotengine.org/en/stable/tutorials/physics/physics_introduction.html#collision-layers-and-masks ·
  `RayCast2D` and `PhysicsRayQueryParameters2D` (`target_position`, `exclude`, `hit_from_inside`):
  https://docs.godotengine.org/en/stable/classes/class_physicsrayqueryparameters2d.html
- **Unity layers and Physics2D queries** — the layer collision matrix (a non-goal: masks say the same per value),
  `Physics2D.Raycast`/`RaycastNonAlloc` into a caller array, `queriesStartInColliders`, `OverlapPoint`/`OverlapBox`,
  `BoxCast` (spec 019): https://docs.unity3d.com/ScriptReference/Physics2D.Raycast.html
- **Box2D** (MIT) — category/mask bits with a symmetric rule (our documented variant), ray casts reporting a fraction
  and ignoring shapes that contain the origin (spec 019; ideas only, no code): https://box2d.org/documentation/md_simulation.html
- **John Amanatides and Andrew Woo, "A Fast Voxel Traversal Algorithm for Ray Tracing"** (Eurographics 1987) —
  visiting every cell a ray crosses, in order; our grid walk is written from the paper, with exact ratio comparisons
  instead of accumulated `tMax` (spec 019): http://www.cse.yorku.ca/~amana/research/grid.pdf
- **Tiled custom enums** — "allow multiple values" saved as a number stores flags as bits, which is how a level sets
  `collision_layers` (spec 019): https://doc.mapeditor.org/en/stable/manual/custom-properties/
- **TOML 1.0** (MIT-licensed specification) — the syntax our data files are a strict subset of; our reader is our
  own (no code from TOML libraries): https://toml.io/en/v1.0.0
- **EnTT** (Michele Caini, MIT) — entities as an index and a version recycled with the version incremented, components
  in sparse sets with pointer stability on request (`in_place_delete`): our worlds take the stable variant, with
  16 + 16-bit handles and lowest-free reuse (spec 020, D35); ideas only, no code:
  https://skypjack.github.io/entt/md_docs_2md_2entity.html · https://skypjack.github.io/2019-05-06-ecs-baf-part-3/
- **flecs** (Sander Mertens) and **Bevy** — archetype tables (components move between them; rejected for a 2D
  homebrew game) and Bevy's opt-in sparse-set storage for components added and removed often:
  https://ajmmertens.medium.com/building-an-ecs-2-archetypes-and-vectorization-fe21690805f9 ·
  https://bevy-cheatbook.github.io/patterns/component-storage.html
- **Godot groups and `queue_free`** — `add_to_group`, `get_nodes_in_group`, `call_group` (our tags,
  `find`, `each_tagged`) and deferred deletion (our destroy is immediate with the slot quarantined until a flush):
  https://docs.godotengine.org/en/stable/tutorials/scripting/groups.html
- **Unity tags and `ObjectPool<T>`** — `CompareTag` without string allocation, `FindGameObjectsWithTag`;
  `Get`/`Release` with an Editor-only double-release check (ours: by generation, in every build) and growth up to a
  maximum (ours: fixed): https://docs.unity3d.com/ScriptReference/Pool.ObjectPool_1-ctor.html
- **Robert Nystrom, *Game Programming Patterns*** — Component, Object Pool and Data Locality (spec 020):
  https://gameprogrammingpatterns.com/object-pool.html · https://gameprogrammingpatterns.com/data-locality.html
- **Richard Fabian, *Data-Oriented Design*** (existence-based processing, a table per kind) and **Niklas Gray
  (Bitsquid), "Building a data-oriented entity system"** (index + generation handles over fixed arrays):
  https://www.dataorienteddesign.com/dodbook/ ·
  http://bitsquid.blogspot.com/2014/08/building-data-oriented-entity-system.html
- **Unity prefabs** — a stored GameObject instantiated many times, instance overrides, prefab variants (spec 021 keeps
  per-copy overrides and variants, without the editor's live link): https://docs.unity3d.com/Manual/Prefabs.html ·
  https://docs.unity3d.com/Manual/PrefabInstanceOverrides.html · https://docs.unity3d.com/Manual/PrefabVariants.html
- **Godot packed scenes** — `PackedScene.instantiate()`, inherited scenes as variants (children wait for a scene tree
  spec): https://docs.godotengine.org/en/stable/classes/class_packedscene.html ·
  https://docs.godotengine.org/en/stable/getting_started/step_by_step/instancing.html
- **flecs prefabs** — prefab entities instantiated with `IsA`, components copied by default (`OnInstantiate,
  Override`; spec 021 takes only the copy) and prefab variants:
  https://github.com/SanderMertens/flecs/blob/master/docs/PrefabsManual.md ·
  https://github.com/SanderMertens/flecs/blob/master/docs/Relationships.md
- **Bevy bundles** — a typed group of components inserted together (the code-declared, value-based shape of a
  prefab): https://bevy-cheatbook.github.io/programming/bundle.html
- **Tiled custom properties and classes** — object classes and properties, the designer's side of naming prefabs
  (templates are not supported): https://doc.mapeditor.org/en/stable/manual/custom-properties/ ·
  https://doc.mapeditor.org/en/stable/manual/using-templates/
- **Robert Nystrom, *Game Programming Patterns*, Prototype** — spawning by copying a prototype, data-driven prototypes
  with a base (spec 021's variants and data-asset values): https://gameprogrammingpatterns.com/prototype.html

Pathfinding on grids (spec 025, D40) — written from the published algorithms and the engines' documentation; no code
copied:

- **Godot `AStarGrid2D` / `AStar2D`** — solid cells and weight scales, regions, the four diagonal modes, partial paths
  (the names of spec 025's `Diagonals`; `AStar2D`'s arbitrary graphs are a non-goal):
  https://docs.godotengine.org/en/stable/classes/class_astargrid2d.html ·
  https://docs.godotengine.org/en/stable/classes/class_astar2d.html
- **A\* Pathfinding Project** (Unity) grid graphs — neighbours, corner cutting, penalties, erosion for bigger agents
  (agent footprints): https://arongranberg.com/astar/docs/gridgraph.html
- **Unity NavMesh** — a walkable surface baked ahead of time, as path maps are baked from grids:
  https://docs.unity3d.com/Manual/nav-BuildingNavMesh.html
- **Amit Patel, Red Blob Games** — introduction and implementation of A*, tie-breaking, heuristics and their scale,
  flow fields for tower defence: https://www.redblobgames.com/pathfinding/a-star/introduction.html ·
  https://www.redblobgames.com/pathfinding/a-star/implementation.html · http://theory.stanford.edu/~amitp/GameProgramming/ ·
  https://www.redblobgames.com/pathfinding/tower-defense/
- **P. E. Hart, N. J. Nilsson, B. Raphael**, "A Formal Basis for the Heuristic Determination of Minimum Cost Paths",
  IEEE Transactions on Systems Science and Cybernetics, 1968 — A* and admissible heuristics
- **Brian Walker**, "The Incredible Power of Dijkstra Maps" (RogueBasin) — one map toward many goals (distance fields):
  https://www.roguebasin.com/index.php/The_Incredible_Power_of_Dijkstra_Maps
- **Elijah Emerson**, "Crowd Pathfinding and Steering Using Flow Field Tiles", *Game AI Pro*, 2013 — integration
  fields: http://www.gameaipro.com/GameAIPro/GameAIPro_Chapter23_Crowd_Pathfinding_and_Steering_Using_Flow_Field_Tiles.pdf
- **Daniel Harabor, Alban Grastien**, "Online Graph Pruning for Pathfinding on Grid Maps", AAAI 2011 (Jump Point
  Search; a non-goal for weighted grids): https://users.cecs.anu.edu.au/~dharabor/data/papers/harabor-grastien-aaai11.pdf
- **Adi Botea, Martin Müller, Jonathan Schaeffer**, "Near Optimal Hierarchical Path-Finding", Journal of Game
  Development, 2004 (HPA*; a non-goal): https://webdocs.cs.ualberta.ca/~mmueller/ps/hpastar.pdf

Scene transitions (spec 023, D38) — read for their designs; no code copied:

- **Godot** — `change_scene_to_file` with a `CanvasLayer` fade autoload (the overlay that must outlive both scenes):
  https://docs.godotengine.org/en/stable/tutorials/scripting/change_scenes_manually.html
- **Unity** — `SceneManager.LoadSceneAsync` with `allowSceneActivation` and a tweened canvas (the stack change held
  until covered; loading progress): https://docs.unity3d.com/ScriptReference/SceneManagement.SceneManager.LoadSceneAsync.html
- **HaxeFlixel** — `FlxTransitionableState`, `TransitionData` (fade, tiles, in/out per state):
  https://api.haxeflixel.com/flixel/addons/transition/FlxTransitionableState.html
- **Defold Monarch** — screen transitions with per-screen handlers and input focus taken while they play:
  https://github.com/britzl/monarch
- **citro2d** — `C2D_Fade` (rejected: per-system data, invisible to tests) and `C2D_DrawRectSolid`, what the overlay's
  rectangles cost on the 3DS: https://citro2d.devkitpro.org/
- **Butano** (GBA) — window-based iris and fade helpers on fixed hardware:
  https://gvaliente.github.io/butano/

## Real-world 3DS engine write-ups
- Silent Hill 2 → 3DS (C engine on libctru + citro3d, offline asset packer, linear-memory budgeting):
  https://ankush.one/blogs/silent-hill-2-3ds-port/
- sm64coopdx New 3DS Citro3D backend (PICA200 shader/TEV path): https://github.com/antoinebou12/sm64coopdx-switch/pull/26

## Emulators (accuracy checks, not the dev loop)
- **Azahar** (Citra successor), Panda3DS.

## Gap we fill
No maintained project combines: modern C++ engine + documented API + desktop simulator + console-side
companion that drives PC builds + native `.cia` output. cpp3ds came closest and stopped.

## Scene tree and transforms (spec 022)

Written from the published designs, no code copied: Godot's `Node2D`, `Transform2D`, `Node.reparent` and `_ready`
order; Unity's `Transform`, `SetParent(worldPositionStays)` and `lossyScale`; Bevy's `Transform`/`GlobalTransform`
propagation; flecs' `ChildOf` and cascading deletes; Michele Caini's "ECS back and forth" part 4 (EnTT hierarchies);
Bitsquid's transform component; Robert Nystrom, *Game Programming Patterns*, "Dirty Flag" (links in
[spec 022](../specs/022-scene-tree-and-transforms/spec.md#prior-art)).

## Particles (spec 024)

Written from the published designs, no code copied: Godot's `CPUParticles2D`/`GPUParticles2D` (amount, lifetime,
one-shot, preprocess, local coordinates, fract delta, draw order, emission shapes, colour ramp, scale curve,
sprite-sheet animation); Unity's Particle System modules (main: max particles, simulation space, prewarm, emitter
velocity; emission: rate over time and bursts with cycles and intervals; shape; colour and size over lifetime; texture
sheet animation); LÖVE's `ParticleSystem` (a fixed buffer, `emit`, emission areas, up to 8 colours and sizes); Phaser 3's
`ParticleEmitter` (flow and explode modes, `explode(count, x, y)`); Robert Nystrom, *Game Programming Patterns*,
"Object Pool" and "Data Locality" (links in [spec 024](../specs/024-particles/spec.md#prior-art)).

## Save data with versions (spec 027)

Written from the published designs, no code copied (links in
[spec 027](../specs/027-save-versions/spec.md#prior-art)):
- **Protocol Buffers**: field numbers that never change or get reused (`reserved`), defaults for missing fields,
  unknown fields skipped by their wire type — https://protobuf.dev/programming-guides/proto3/
- **FlatBuffers**: explicit field ids and `deprecated` instead of removal — https://flatbuffers.dev/evolution/
- **Serde** (Rust): `#[serde(default)]` for fields missing from old data, versioned structs converted in a chain —
  https://serde.rs/field-attrs.html
- **Godot** `ConfigFile`, `ResourceSaver` and the "Saving games" tutorial —
  https://docs.godotengine.org/en/stable/tutorials/io/saving_games.html
- **Unity** `JsonUtility` (missing fields keep their defaults) and `PlayerPrefs` —
  https://docs.unity3d.com/ScriptReference/JsonUtility.html
- **Butano** (GBA) `bn::sram`: a struct copied whole, what raw `nnh::save` does —
  https://gvaliente.github.io/butano/namespacebn_1_1sram.html
- **Pokémon Generation III** save blocks (alternating, checksummed, the previous one used on failure) —
  https://bulbapedia.bulbagarden.net/wiki/Save_data_structure_(Generation_III)
- **GBATEK**, GBA cart backup SRAM/FRAM — https://problemkaputt.de/gbatek-gba-cart-backup-sram-fram.htm

## 3D foundation (spec 026)

Written from published documentation only; nothing copied.

- [citro3d](https://github.com/devkitPro/citro3d) headers (`c3d/light.h`, `c3d/lightlut.h`, `c3d/maths.h`,
  `c3d/renderqueue.h`) and devkitPro's [3ds-examples graphics/gpu](https://github.com/devkitPro/3ds-examples/tree/master/graphics/gpu)
  (`fragment_light`: the normal-quaternion method of our vertex shader; `composite_scene`: citro3d between citro2d's
  flush and prepare); [citro2d base.c](https://github.com/devkitPro/citro2d/blob/master/source/base.c).
- [3dbrew GPU/Fragment Lighting](https://3dbrew.org/wiki/GPU/Fragment_Lighting), [GPU/Internal Registers](https://www.3dbrew.org/wiki/GPU/Internal_Registers),
  [GPU/Shader Instruction Set](https://www.3dbrew.org/wiki/GPU/Shader_Instruction_Set).
- [Khronos glTF 2.0](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html) and
  [`KHR_materials_unlit`](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_unlit);
  cgltf and tinygltf (MIT) read as references only.
- Fabian Giesen, [Triangle rasterization in practice](https://fgiesen.wordpress.com/2013/02/08/triangle-rasterization-in-practice/)
  (fixed-point edges, the top-left rule); Chris Hecker, [perspective texture mapping](https://www.chrishecker.com/Miscellaneous_Technical_Articles).
- David Eberly, "A Fast and Accurate Algorithm for Computing SLERP" (JGT 2011): the recurrence of `slerp`, here with
  16 terms and a fitted last coefficient.
- [raylib](https://www.raylib.com/cheatsheet/raylib_cheatsheet_v4.0.pdf) (`Camera3D`, `GenMesh*`, `GetWorldToScreen`),
  [Godot's 3D transparency limits](https://docs.godotengine.org/en/stable/tutorials/3d/3d_rendering_limitations.html) and
  [Unity's Camera.WorldToScreenPoint](https://docs.unity3d.com/ScriptReference/Camera.WorldToScreenPoint.html): API shapes.

## UI widgets (spec 028)

Written from the published designs, no code copied:

- **Godot** `Control` nodes — anchors and offsets, size flags with stretch ratios, containers, `NinePatchRect`, themes
  with per-state styleboxes, focus neighbours —
  [Control](https://docs.godotengine.org/en/stable/classes/class_control.html),
  [Using containers](https://docs.godotengine.org/en/stable/tutorials/ui/gui_containers.html),
  [Keyboard/Controller navigation and focus](https://docs.godotengine.org/en/stable/tutorials/ui/gui_navigation.html),
  [NinePatchRect](https://docs.godotengine.org/en/stable/classes/class_ninepatchrect.html).
- **Unity** uGUI — `Selectable` navigation (automatic, explicit, wrap), layout groups, 9-sliced sprites —
  [Navigation options](https://docs.unity3d.com/Packages/com.unity.ugui@1.0/manual/script-SelectableNavigation.html),
  [Auto layout](https://docs.unity3d.com/Packages/com.unity.ugui@1.0/manual/comp-UIAutoLayout.html),
  [9-slicing](https://docs.unity3d.com/Manual/9SliceSprites.html).
- **Clay** (Nic Barker): layout in a fixed arena without allocation — https://github.com/nicbarker/clay
- **microui** and **Dear ImGui**: immediate-mode UIs, considered and not chosen —
  https://github.com/rxi/microui, https://github.com/ocornut/imgui
- Console menu conventions (DS/3DS games): held-direction repeat, wrap on a fresh press only, L/R pages, A confirm /
  B back, touch and D-Pad parity on the lower screen.
