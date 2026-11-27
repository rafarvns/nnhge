# audio — sound effects, music and the mixer

Header: `<nnh/audio.hpp>` · Spec: [008](../../specs/008-audio/spec.md) · Files: [assets.md](assets.md)

> Status: **implemented** (spec 008): PC simulator (SDL 3) and 3DS (DSP), verified on an Old 3DS XL
> (2026-11-18). Example: [`examples/audio`](../../examples/audio/main.cpp).

Put WAV or Ogg Vorbis files in the game's [assets](assets.md) folder. Load each one once, then play it as
many times as you like. Every play is a **voice** with its own volume, pan and pitch.

```cpp
auto loaded = nnh::load_sound("sfx/jump");         // assets/sfx/jump.wav (or .ogg)
if (!loaded) {
    NNH_LOG_ERROR(game, "%s", loaded.error().message);
    return;
}
nnh::Sound jump = std::move(loaded.value());
nnh::audio::play(jump);                                                     // fire and forget
nnh::Voice engine = nnh::audio::play(motor, {.volume = 0.6f, .loop = true}); // keep the handle to change it
nnh::audio::set_pitch(engine, 1.0f + speed / 200.0f);
```

## Sounds

| API | |
|-----|-|
| `Result<Sound> load_sound(path)` | `path` without extension: `"sfx/jump"` → `sfx/jump.wav`, else `sfx/jump.ogg`. Decodes the whole file into memory; load time only |
| `Sound::valid()`, `duration()`, `reset()` | loaded; length in seconds; free it now (its voices stop first). Move-only, like `Texture` |

- Formats: WAV (PCM 8 or 16-bit) and Ogg Vorbis, mono or stereo, 8000–48000 Hz. The **build** checks
  every `.wav`/`.ogg` in the assets folder and fails, naming the file, for anything else (24-bit,
  3 channels, 96 kHz, Opus…; [cmake.md](../tech/cmake.md#common-changes)).
- A decoded sound is 16-bit and at most **4 MB** (about 65 s of mono at 32 kHz). Longer audio is music
  (streamed, see [Music](#music)). On the 3DS, 32 kHz or less saves memory; the hardware plays any rate.
- Memory: decoded samples live in `Linear` memory (the 3DS DSP reads them), counted under the tag `audio`
  ([memory.md](memory.md)).

## Voices

| API | |
|-----|-|
| `Voice audio::play(sound, options = {})` | starts a voice. `PlayOptions`: `volume` 0..1, `pan` -1..1, `pitch` 0.25..4, `loop`, `group`, `priority` 0..255 (default 128) |
| `audio::stop(v)`, `set_paused(v, bool)` | stop (no event), pause or resume |
| `audio::set_volume(v, x)`, `set_pan(v, x)`, `set_pitch(v, x)` | change a playing voice |
| `audio::playing(v)` | true from `play()` until it ends, is stopped or replaced (paused counts as playing) |

- A `Voice` is a small value (`{index, generation}`). Once its voice ends, every call with that handle
  does nothing, even if the voice slot plays another sound later.
- **Pan is a balance**: at 0 both speakers play at full volume; +1 silences the left one.
- **Pitch** changes speed and pitch together (2 = one octave up, twice as fast).
- Out-of-range values are clamped, never an error.
- **Voice limit**: `AppConfig::audio.voices` (default 16, at most 22). When every voice is busy, a new
  sound replaces the lowest-priority voice, the oldest one among equal priorities. If every busy voice
  has a *higher* priority, the new sound is dropped: `play()` returns a `{}` voice and the first drop of
  each sound is logged on the `audio` channel.

## Groups and master volume

Every voice belongs to a group (`PlayOptions::group`): `AudioGroup::Effects` (default), `Music` or
`Dialogue`. A voice plays at its volume × its group's volume × the master volume.

| API | |
|-----|-|
| `audio::set_volume(group, x)`, `volume(group)` | the options menu's sliders (0..1, default 1) |
| `audio::set_master_volume(x)`, `master_volume()` | everything |
| `audio::set_paused(group, bool)`, `paused(group)` | e.g. pause effects and dialogue during a pause menu; a voice paused on its own stays paused when its group resumes |
| `audio::available()` | false when there is no audio output (see below) |

## Music

One track at a time — two during a crossfade — **streamed** from the assets while the game runs:
memory doesn't grow with its length (the platform keeps ~0.5 s queued; an Ogg track also needs ~256 KB
for its decoder while it plays). Music plays in `AudioGroup::Music`.

```cpp
nnh::music::play("music/forest", {.fade_in = 1.0f});   // assets/music/forest.ogg, loops by default
nnh::music::crossfade("music/boss", 2.0f);              // forest fades out while boss fades in
nnh::music::play("music/jingle", {.loop = false});     // MusicFinished when it has played
nnh::music::stop(0.5f);                                 // fade out, no event
```

| API | |
|-----|-|
| `music::play(path, options = {})` | replaces any music at once. `Options`: `loop` (default true), `loop_start`, `volume`, `fade_in` (seconds) |
| `music::crossfade(path, seconds, options = {})` | the current track fades out while `path` fades in; without music, a fade-in |
| `music::stop(fade_out = 0)` | stop now or fade out; no `MusicFinished` |
| `music::set_paused(bool)`, `paused()` | also paused while `AudioGroup::Music` is paused |
| `music::set_volume(x)` | the track's own volume (fades and the Music group multiply it) |
| `music::playing()`, `position()` | a current track plays (paused counts); seconds into it |

- **Loop points**: a looping track restarts at `loop_start` (in frames: samples per channel), so an
  intro plays once. With `loop_start = 0`, an Ogg file's `LOOPSTART` comment tag is used when it has one
  (many music tools write it). The jump is sample-exact and doesn't decode the intro again.
- `position()` wraps at the loop: after the end it continues from the loop point.
- A failed `play`/`crossfade` (missing file, bad loop point…) leaves the current music playing.
- Music formats are those of sounds (WAV or Ogg Vorbis; the build checks them). A WAV's header must fit
  in its first 4 KB.
- Streaming happens in `nnh::frame()`: a frame longer than ~0.5 s (e.g. loading many files) can make the
  music stutter. Load at the start of a scene.
- Fading the music with a scene transition: `music::stop(transition::seconds(transition::timing().out))` when it
  starts, `music::play(..., {.fade_in = ...in...})` from its `on_covered` ([transition.md](transition.md#music)).

## Events

| Event | When | Payload |
|-------|------|---------|
| `nnh::SoundFinished` | in `nnh::frame()`, after a voice played to its end (not after `stop()`, nor when replaced) | `Voice voice` |
| `nnh::MusicFinished` | in `nnh::frame()`, after a track that doesn't loop has played its last frame (not after `stop()`, `play()` replacing it, or a crossfade fading it out) | — |

```cpp
nnh::Subscription s = nnh::events::subscribe<nnh::SoundFinished>([&](const nnh::SoundFinished& e) {
    if (e.voice == intro) start_level();
});
```

## Without audio output

A 3DS without its DSP firmware (`sdmc:/3ds/dspfirm.cdc`, dumped once by the user) or a PC without a
sound device still runs the game: `init()` succeeds, the reason is logged once, `audio::available()` is
false, and everything above keeps working **silently**. Sounds load, voices play and end on time (by the
clock), music keeps time (its `position()` advances, loops wrap) and `SoundFinished`/`MusicFinished`
are published as usual. Games need no special case.

## Capacities and errors

`AppConfig::audio`: `.voices` (1–22, default 16) and `.max_sounds` (sounds loaded at once, default 128,
about 40 bytes each).

| `ErrorCode` (`load_sound`, `music::play`) | When |
|----------------------------|------|
| `NotFound` | no `.wav` or `.ogg` with that path in the assets |
| `InvalidArgument` | the path breaks the [asset path rules](assets.md#paths), or is null; a `loop_start` past the end |
| `Unsupported` | another encoding, bit depth, channel count or rate; or called before `nnh::init()` |
| `Corrupted` | the file is damaged |
| `LimitReached` | `max_sounds` are loaded, or the decoded sound is over 4 MB |
| `OutOfMemory` | no memory to decode or for the samples |

## Hot reload (spec 031)

In a Debug/Development game under `nnh run`, saving a `.wav`/`.ogg` that a `Sound` was loaded from reloads it at the start
of a frame: the same `Sound` handle now has the new samples (`duration()` too), the voices playing it stop in that frame —
each with its `SoundFinished` — and the next `audio::play` uses the new samples. The old platform samples are freed at the
next batch or at shutdown. A file that doesn't decode refuses the whole batch and the old sound stays. Music doesn't reload
yet (a reloaded track is announced: `assets::reloaded()`). See [assets.md](assets.md#hot-reload).
