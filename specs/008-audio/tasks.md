# Tasks 008: Audio — sound effects, streaming music, mixer

Status: draft
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task: one or a few TDD loops. `[P]` = can be done in parallel with the previous task.

## Stage 1 — readers and the build check

- [ ] **T1.1** WAV reader (bounded, chunk skipping, formats) — files: `core/src/audio/wav_reader.{hpp,cpp}` — test: `tests/audio/test_wav_reader.cpp` + fixtures `tests/data/sounds/` — covers FR-1, FR-10
- [ ] **T1.2** vendor stb_vorbis (pinned, license, REFERENCES); OGG reader on the push-data API with an arena, `LOOPSTART`, page offsets for the loop point — files: `third_party/stb_vorbis/`, `core/src/audio/ogg_reader.{hpp,cpp}` — test: `tests/audio/test_ogg_reader.cpp` — covers FR-1, FR-3, FR-10
- [ ] **T1.3** [P] fuzz targets `wav`, `ogg` (mini-fuzz + libFuzzer) — files: `tests/fuzz/` — covers FR-10
- [ ] **T1.4** [P] `nnh_check_sound.cmake` in the assets pipeline — test: ctest `sound_check_*` (valid, 24-bit, 3 channels, not OGG) — covers FR-1

## Stage 2 — contract, fake, effects and mixer

- [ ] **T2.1** contract `audio.hpp` (+ `asset_open/read_at/close`); fake recorder; pc/3ds stubs returning "no audio" — test: conformance `audio_init_or_silent`, `asset_stream_reads_ranges` — covers FR-8, FR-10
- [ ] **T2.2** `load_sound` (path + `.wav`/`.ogg`, decode to PCM16, `sample_create`, tag `audio`, `max_sounds`) — files: `core/src/audio/sounds.cpp` — test: `tests/audio/test_sounds.cpp` — covers FR-1, FR-2
- [ ] **T2.3** voices: play/stop/pause/params, handles, stealing by priority then age — files: `core/src/audio/voices.{hpp,cpp}` — test: `test_voices.cpp` — covers FR-2, FR-5
- [ ] **T2.4** mixer: groups, master, pan law, group pause, fades — files: `core/src/audio/mixer.{hpp,cpp}` — test: `test_mixer.cpp` — covers FR-4
- [ ] **T2.5** frame hook, events, unavailable audio, shutdown — files: `core/src/audio/audio.cpp`, `app.cpp`, `<nnh/audio.hpp>` — test: `test_audio_events.cpp`, `test_audio_unavailable.cpp`, `test_audio_lifecycle.cpp` — covers FR-6, FR-7, FR-8

## Stage 3 — music streaming

- [ ] **T3.1** music: open, top up stream buffers each frame, WAV and OGG, loop + loop point, pause/stop, position — files: `core/src/audio/music.{hpp,cpp}` — test: `test_music.cpp` — covers FR-3
- [ ] **T3.2** fade-in/out and crossfade on two streams; `MusicFinished` — test: `test_music.cpp` — covers FR-3, FR-6

## Stage 4 — platforms and the example

- [ ] **T4.1** PC: SDL audio stream + software mixer (resampling, gain ramps), asset streams — files: `platforms/pc/src/audio_pc.cpp`, `audio_mixer_pc.cpp` — test: conformance on pc; listened to in the example — covers FR-9
- [ ] **T4.2** 3DS: ndsp voices and streams, linear buffers, firmware missing → silent, asset streams — files: `platforms/3ds/src/audio_3ds.cpp` — test: conformance on 3ds (build), example on hardware — covers FR-7, FR-8, FR-9
- [ ] **T4.3** `examples/audio` + `make_sounds.py` (WAV effects, OGG music; `pip install soundfile` once, maintainer OK) — covers AC-3

## Stage 5 — verification and docs

- [ ] **T5.1** docs: new `docs/api/audio.md`; `assets.md`, `cmake.md` (sound check), `devkitpro-3ds.md` (ndsp, dspfirm), `sdl3.md`, decision D21, `REFERENCES.md`, CLAUDE.md — covers AC-6
- [ ] **T5.2** budgets, `hello` size and startup, Release strings, Linux gcc + ASan/UBSan + libFuzzer on VM 120 — covers AC-2, AC-5, AC-6
- [ ] **T5.3** maintainer on an Old 3DS: `examples/audio` (music, overlapping effects, crossfade, HOME, lid), 60 fps with music (overlay), then without `dspfirm.cdc` — covers AC-3, AC-4, AC-5
- [ ] **T5.4** `docs/ROADMAP.md` M4; spec, plan and tasks status → implemented
