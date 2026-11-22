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
- **TOML 1.0** (MIT-licensed specification) — the syntax our data files are a strict subset of; our reader is our
  own (no code from TOML libraries): https://toml.io/en/v1.0.0

## Real-world 3DS engine write-ups
- Silent Hill 2 → 3DS (C engine on libctru + citro3d, offline asset packer, linear-memory budgeting):
  https://ankush.one/blogs/silent-hill-2-3ds-port/
- sm64coopdx New 3DS Citro3D backend (PICA200 shader/TEV path): https://github.com/antoinebou12/sm64coopdx-switch/pull/26

## Emulators (accuracy checks, not the dev loop)
- **Azahar** (Citra successor), Panda3DS.

## Gap we fill
No maintained project combines: modern C++ engine + documented API + desktop simulator + console-side
companion that drives PC builds + native `.cia` output. cpp3ds came closest and stopped.
