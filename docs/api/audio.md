# audio — sound effects and the mixer

Header: `<nnh/audio.hpp>` · Spec: [008](../../specs/008-audio/spec.md) · Files: [assets.md](assets.md)

> Status: **in progress** (spec 008). Sound effects, voices, groups and `SoundFinished` work, and are
> tested on the fake platform. Streaming music comes in stage 3. The PC and 3DS outputs come in stage 4:
> until then `audio::available()` is false there and games run silently with the same behaviour.

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
  (streamed, stage 3). On the 3DS, 32 kHz or less saves memory; the hardware plays any rate.
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

## Events

| Event | When | Payload |
|-------|------|---------|
| `nnh::SoundFinished` | in `nnh::frame()`, after a voice played to its end (not after `stop()`, nor when replaced) | `Voice voice` |

```cpp
nnh::Subscription s = nnh::events::subscribe<nnh::SoundFinished>([&](const nnh::SoundFinished& e) {
    if (e.voice == intro) start_level();
});
```

## Without audio output

A 3DS without its DSP firmware (`sdmc:/3ds/dspfirm.cdc`, dumped once by the user) or a PC without a
sound device still runs the game: `init()` succeeds, the reason is logged once, `audio::available()` is
false, and everything above keeps working **silently**. Sounds load, voices play and end on time (by the
clock), and `SoundFinished` is published as usual. Games need no special case.

## Capacities and errors

`AppConfig::audio`: `.voices` (1–22, default 16) and `.max_sounds` (sounds loaded at once, default 128,
about 40 bytes each).

| `ErrorCode` (`load_sound`) | When |
|----------------------------|------|
| `NotFound` | no `.wav` or `.ogg` with that path in the assets |
| `InvalidArgument` | the path breaks the [asset path rules](assets.md#paths), or is null |
| `Unsupported` | another encoding, bit depth, channel count or rate; or called before `nnh::init()` |
| `Corrupted` | the file is damaged |
| `LimitReached` | `max_sounds` are loaded, or the decoded sound is over 4 MB |
| `OutOfMemory` | no memory to decode or for the samples |
