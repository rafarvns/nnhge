# Spec 008: Audio — sound effects, streaming music, mixer

Status: implemented (approved and implemented 2026-11-18)
Author: NNHGE maintainers · Date: 2026-11-18
Depends on: [spec 007](../007-assets-and-romfs/spec.md) (sounds are assets), [spec 002](../002-core-foundation/spec.md)
(memory tags, `Result`, logs), [spec 001](../001-event-bus-and-scenes/spec.md) (events). Implements ROADMAP M4.

## Problem and goal

A game made with NNHGE today is silent: there is no way to play a sound effect or music. After this spec
a game puts sound files in its assets folder, loads short **sound effects** into memory and plays them
with one call (many at once, with volume, pan and pitch), plays **music** streamed from the assets while
the game runs, and controls the mix through a few volume groups. The same game code sounds the same in
the simulator and on the 3DS, and a console without the DSP firmware still runs the game, silently.

## User stories

- As a game developer, I want to play a jump sound with one call, even several times at once, so that
  feedback feels immediate.
- As a game developer, I want background music that loops (optionally from a loop point after an intro)
  and changes between scenes with a fade, without loading the whole song into memory.
- As a game developer, I want separate volumes for music and effects so that I can offer them in an
  options menu.
- As a game developer, I want to know when a sound or the music ends, so that I can chain things.
- As a player, I want the game to run even when my console lacks the DSP firmware, and audio to stop
  when I press HOME or close the lid.
- As a contributor, I want the mixer logic tested without a sound card.

## Functional requirements

- **FR-1 Sound files from assets.** Sounds are files in the game's assets folder (spec 007), loaded by
  path like textures: `load_sound("sfx/jump")`. Supported formats: **WAV** (PCM 8/16-bit, mono or
  stereo) and **OGG Vorbis** (see open questions). A bad or unsupported file fails at **build** time
  naming it when the build can tell, otherwise at load time with a `Result` error.
- **FR-2 Sound effects.** A loaded `Sound` is decoded once into memory (RAII, move-only, like `Texture`)
  and can be played many times, overlapping. `play()` returns a handle to that playing instance (a
  **voice**) that can be stopped, paused, and have its volume, pan (left–right) and pitch changed while
  playing; a stale handle is harmless.
- **FR-3 Music.** One music track plays at a time, **streamed** from the assets in small buffers (memory
  does not grow with the song's length). It can loop, optionally from a **loop point** (sample offset)
  so that an intro plays once. It can be paused, resumed, stopped, and faded in/out or cross-faded to a
  new track over a given time.
- **FR-4 Mixer.** Every sound belongs to a **group** (at least `music` and `effects`, plus master) with
  its own volume; the final volume is voice × group × master. Groups can be paused as a whole (e.g. a
  pause menu silences effects).
- **FR-5 Voice limit.** A fixed number of simultaneous voices, set in `AppConfig` at init and the same on
  every system (the 3DS DSP has 24 channels; music uses some). When all voices are busy, a new effect
  replaces the oldest lowest-priority one, or is dropped if its priority is lower; this is logged once.
- **FR-6 State and events.** Queries: is a voice playing, is music playing, current music position.
  Events (delivered in `nnh::frame()`): `SoundFinished` (voice handle) and `MusicFinished` (non-looping
  track ended). No callbacks from the audio thread reach game code.
- **FR-7 Lifecycle.** Audio stops while the app is suspended (HOME menu, lid closed / sleep) and resumes
  where it was. `nnh::shutdown()` stops everything and frees all audio memory.
- **FR-8 Works without audio.** If the system has no audio (3DS without `dspfirm.cdc`, PC without an
  output device), `init` still succeeds: loads and plays succeed and are silent, `audio::available()` is
  false and the reason is logged once. Games never need a special path.
- **FR-9 Same behaviour everywhere.** The simulator mixes with the same voice limit, groups, loop and
  fade rules as the 3DS; only the output device differs (constitution II).
- **FR-10 Testable and safe.** The audio output is part of the platform contract, so the fake platform
  records what would be played (voices, volumes, buffers queued) for unit tests of the mixer, voice
  allocation, fades, streaming and errors. The WAV and OGG readers are bounded and fuzz-tested
  (constitution X).

## Acceptance criteria

- **AC-1** Unit tests through the fake platform cover: voice allocation and stealing by priority, stale
  handles, group and master volume, pause/resume of a group, fades and cross-fades over time, music loop
  with a loop point, end events, behaviour when audio is unavailable, and every load error.
- **AC-2** WAV/OGG readers have unit tests with valid and broken files and fuzz targets (mini-fuzz in
  `nnh_tests`, libFuzzer on Linux), clean under ASan/UBSan.
- **AC-3** New example `examples/audio`: music with an intro and loop point, effects on buttons (several
  overlapping), volume groups on screen, a cross-fade between two tracks; runs in the simulator (heard by
  the maintainer) and on an Old 3DS (maintainer), including HOME menu and lid close/open.
- **AC-4** On the 3DS, music streaming keeps 60 fps in `examples/audio` with effects playing (frame time
  shown by the debug overlay); the audio memory is reported by the memory tags.
- **AC-5** A 3DS without the DSP firmware (rename `dspfirm.cdc`) runs `examples/audio` silently with one
  log line. Games without audio don't grow in size or startup time noticeably (measured on `hello`).
- **AC-6** Build budgets still met; `docs/api/audio.md` (new), `assets.md`, `devkitpro-3ds.md`, `sdl3.md`,
  decisions, ROADMAP M4 updated.

## Non-goals

- 3D/positional audio, effects (reverb, filters), the 3DS surround mode, microphone input.
- MIDI, tracker modules (MOD/XM), synthesizers; formats beyond WAV and OGG Vorbis (e.g. MP3, Opus).
- More than one music track at once (layered/adaptive music) — later, if a game needs it.
- Hot reload of sounds; editor tooling; `.cia` (M5).

## Constraints

- Constitution I: one API (`<nnh/audio.hpp>`), no `#ifdef` in games; 3DS-only knobs (if any) through an
  extension header.
- Constitution II: Old 3DS budgets — the ARM11 at 268 MHz decodes music while the game runs; sample memory
  for the DSP must be in linear memory (shared with textures). Memory per sound and the streaming buffers
  are fixed and reported by tag (`audio`).
- Constitution III: the decoder is small and compiles fast; no heavy dependency in public headers.
- Constitution IV: game code sees events and queries only; the audio thread never calls game code.
- Constitution V: no allocation per frame or per `play()`; loading is a load-time operation.
- Constitution IX: decoders under a zlib-compatible license, credited in `docs/REFERENCES.md`.
- Constitution X: file readers bounded and fuzz-tested.

## Open questions

Answered by the maintainer (2026-11-18): the proposals, all accepted (music format after comparing the costs below).

- [x] **Music format** — proposal: **OGG Vorbis** in the assets, decoded on the CPU while streaming with
  a vendored public-domain/MIT decoder (stb_vorbis-style; no portlibs install needed, same code on PC and
  3DS). Alternative: the build converts music to DSP-ADPCM, which the 3DS decodes in hardware (no CPU
  cost) but files are ~2–3× bigger than OGG (32 KB/s for 32 kHz stereo vs. ~12–16 KB/s), needs an
  encoder tool we'd write (the CMake scripts can't do it), and the PC needs an ADPCM decoder too.
  Another alternative: the devkitPro portlibs `libogg` + Tremor (BSD-3-Clause, compatible with zlib but
  their notice must ship with games), one `pacman` command per machine, but every contributor, CI and
  the Linux VM must install them, and the PC side needs a second copy of the library.
- [x] **Effect formats** — proposal: WAV and OGG both accepted for effects, decoded to 16-bit PCM at
  load time (OGG for long effects saves space in the romfs).
- [x] **Sample rate** — proposal: keep each file's rate and let the mixer resample (the 3DS DSP does it
  per channel); the build warns above 48 kHz. Alternative: convert everything at build time to one rate.
  (Implementation note: the build **refuses** files above 48 kHz instead of warning, since the readers
  reject them at run time; the message suggests 32 kHz.)
- [x] **Voice count** — proposal: 16 effect voices by default (configurable up to 22), 2 DSP channels
  reserved for music.
- [x] **Groups** — proposal: fixed `master`, `music`, `effects`, plus `dialogue` (named so it is not confused with a playing voice), so menus can
  expose the usual sliders; no user-defined groups yet.
- [x] **Loop point source** — proposal: an argument when playing (`loop_start` in samples), and the
  `LOOPSTART` comment tag in OGG files read automatically when present.
