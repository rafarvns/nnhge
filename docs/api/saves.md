# saves — save data with versions

Header: `<nnh/saves.hpp>` · Spec: [027](../../specs/027-save-versions/spec.md) ·
Plan: [027](../../specs/027-save-versions/plan.md) · Decisions: [D45, D46](../architecture/decisions.md) ·
Example: [`examples/saves`](../../examples/saves/progress.hpp) · Raw saves: [fs.md](fs.md#saves) ·
Value types: [data.md](data.md)

A game describes its save struct **once in C++**, giving each saved member a stable numeric **id**, and stores and loads
it with one call. Fields can be added, removed, renamed, reordered and widened between releases without code; a **save
version** and ordered **migration steps** (v1 → v2 → v3) cover what can't be automatic. The file layout is the
engine's (little-endian, no padding, fields by id), so the same save loads on PC and 3DS, inside spec 002's `nnh::save`
container: CRC-32, atomic write through a temp file, the previous file kept as `.bak`. Loading validates the whole file
before touching the struct, never crashes on a damaged or edited file, never allocates, and reports what it adjusted.

Optional (constitution XI): a game that doesn't include the header pays nothing, and raw
[`nnh::save::read`/`write`](fs.md#saves) keep working unchanged. The header includes only `<nnh/data.hpp>` (the value
types `data::String`, `data::List`, `data::Ref`; including it links no data-asset code) and `<nnh/result.hpp>`.

```cpp
struct Progress {
    nnh::data::String<24> name;
    std::uint16_t level = 1;
    std::uint64_t play_frames = 0;
    nnh::data::List<Item, 64> inventory;   // Item has its own NNH_SAVE_STRUCT
};
// At namespace scope, in Progress's own namespace (a header, or one source file):
NNH_SAVE_TYPE(Progress, "progress", NNH_SAVE_FIELD(name, 1), NNH_SAVE_FIELD(level, 2),
              NNH_SAVE_FIELD(play_frames, 5), NNH_SAVE_FIELD(inventory, 9));

nnh::save::Buffer<Progress> g_buffer;     // max_size<Progress>() bytes: static or in a scene, not on the stack
nnh::save::store("slot1.sav", progress, g_buffer);
if (auto r = nnh::save::load("slot1.sav", progress, g_buffer); !r) start_new_game();
```

## Declaring a save type

One macro per struct, at **namespace scope in the struct's own namespace**, naming the struct **without
qualification** (the engine finds the declaration by argument-dependent lookup: no registry, no static initialiser, no
build step). It can live in a header (every file that includes it can store the type) or in one source file (the
other files see only the struct: the [settings](../../examples/saves/settings.cpp) of the example). Arguments in any
order:

| Macro | Meaning |
|-------|---------|
| `NNH_SAVE_TYPE(Type, "key", ...)` | a save type: a file's root, with a key (1–16 of `a-z 0-9 _`, starting with a letter) |
| `NNH_SAVE_STRUCT(Type, ...)` | a struct inside saves (nested, or the items of an array or list): fields and retired ids only |
| `NNH_SAVE_SUMMARY(Type, "key", version, ...)` | a partial view of a save type for "Load game" screens ([Slots](#slots-summaries-and-settings)) |
| `NNH_SAVE_FIELD(member, id)` | a saved member and its id, 1..65 535, unique in its struct |
| `NNH_SAVE_RETIRED(id, ...)` | ids of fields removed in earlier versions: never reusable, skipped silently in old files |
| `NNH_SAVE_VERSION(n)` | the type's current save version (default 1) |
| `NNH_SAVE_STEP(from, function)` | `void function(Type& value, const nnh::save::OldFields& old)` converts version `from` into `from + 1` |

The struct must be a plain aggregate, standard-layout and trivially copyable, at most 64 KiB. A nested struct needs its
own `NNH_SAVE_STRUCT` declared **before** the declarations that use it. A struct may also be a spec 014 data type: the
two declarations don't interact.

**Ids** identify fields in files; member names never appear there. Renaming a member, moving it, adding or removing
other fields never changes what an id means. **Never give an id a new meaning**: when a field goes away, list its id in
`NNH_SAVE_RETIRED` (reusing it is then a build error). Each nested struct has its own ids.

**Build errors** (each names the fix): an unknown member; a pointer, reference, `std::string`, union, `char`, `long
double`, const or non-trivially-copyable member; an id of 0 or above 65 535, an id twice, a retired id used or listed
twice; a nested struct without its declaration; more than 128 fields, more than 64 retired ids, more than 8 struct
levels; a root over 64 KiB, an array over 65 535 items, a largest encoding over 256 KiB; a bad key; version 0, a step
from a version not below the current one, two steps for one version, **a version below the current one without its
step** (the chain is checked at compile time); a version or step in a struct or summary; storing a struct, a summary or
an undeclared type. They are compiled as tests (`tests/saves/compile_fail/`).

## Kinds and limits

| Member type | In the file | Notes |
|-------------|-------------|-------|
| `bool` | 1 byte, 0/1 | any non-zero byte is written as 1 |
| `std::int8_t` … `std::uint64_t` | 1, 2, 4, 8 bytes | use the `<cstdint>` types (`long`'s size differs between systems) |
| `float`, `double` | their IEEE bits | non-finite values are written as they are and load as the default |
| enums | their underlying integer | with [`NNH_DATA_ENUM`](data.md) names, a value outside them loads as the default |
| `nnh::Vec2`, `nnh::Color` | 8, 4 bytes | |
| `data::String<N>` | its length (1 byte) and UTF-8 bytes | N ≤ 255; a string that isn't UTF-8 refuses to store |
| `data::Ref<T>`, `TextureRef`, `SoundRef`, `MusicRef` | the 32-bit path hash | valid across builds as long as the asset's path |
| a struct with `NNH_SAVE_STRUCT` | its fields, by its own ids | up to 8 levels, the root included |
| `T[N]` | N items, packed | N ≤ 65 535 |
| `data::List<T, N>` | its count and items | N ≤ 256; a `List<std::uint8_t, N>` is a blob (an `ActionSet`'s `save_to` bytes) |

Arrays and lists of numbers are one copy; a list of structs is written as one template (its fields' kinds and ids) and
rows, not a tag per item field. Every declared field is written, defaults included.

## The file and its guarantees

`store` encodes into the caller's buffer and hands it to spec 002's unchanged `nnh::save::write`: **one** platform
write of the temp file, then the previous file becomes `<path>.bak` and the temp file is renamed into place — the same
five file operations as a raw save. A power cut at any moment leaves the new save or the previous one loadable (tested
by cutting the power after every file operation on the fake platform). The `NNHS` container has the CRC-32; inside, a
16-byte structured header (`NNSV`, format 1, the key and its hash, the game's save version) and the fields in ascending
id order. Encoding the same values with the same declaration gives **the same bytes on every compiler and on the 3DS**
(the golden files of `tests/data/saves/` are checked byte for byte).

## Loading rules and the report

`load` reads the file, **validates the whole structure first** (every length, count, id, kind, nesting level, string;
linear in the size), checks the key and the version, and only then writes the struct: it becomes `T{}` (your defaults),
gets every field the file has by id, and the migration steps run. On **any error the struct is unchanged**.

| What changed between the file and the struct | What happens |
|-----------------------------------------------|--------------|
| a field added | it keeps its default |
| a field removed | skipped, counted `unknown` (silently when its id is retired) |
| a member renamed or moved | loads by id |
| an integer widened, signedness changed | converts when the value fits, else the default (`dropped`) |
| an integer into `float`/`double` | when exactly representable (`2^24 + 1` into a float: dropped) |
| `float` into `double`; `double` into `float` | always; only when exact |
| array ↔ list, capacity grown | every item loads; an array with fewer items keeps the defaults for the rest |
| a string or sequence longer than its new capacity | cut at a UTF-8 character boundary / the first items kept (`adjusted`) |
| another kind (a number became a struct, one value became an array), a non-finite float, an enum value outside its names, a bool byte other than 0/1 | the default (`dropped`, per value) |
| a nested struct | by its own ids, by the same rules |

The `Report` says: `used_backup`, `version` (the save version that wrote the file), `migrated` (steps run), and the
`unknown`, `dropped` and `adjusted` counts (saturating at 65 535), with the first three paths of each in Debug and
Development builds (`9[3].10`: field 10 of item 3 of field 9). A load that used the backup, dropped or adjusted logs
**one** warning line (`save slot1.sav: backup used (slot1.sav: field 7: list count 900 exceeds the file); v1 -> v3 (2
steps); dropped 1 (8.2)`); a clean load logs nothing.

**Errors**: `NotFound` (no file and no backup: start a new game), `Corrupted` (the file and its backup both unusable),
`InvalidArgument` (a bad path; a save of another type: "a 'settings' save, not 'progress'"), `Unsupported` (a newer
save version: "saved by version 5; this game reads up to 3"; a newer format; a raw `nnh::save` file),
`LimitReached` (the buffer), `IoError`. A missing, unreadable, damaged (CRC) or **structurally broken** main file is
replaced by its `.bak`; another type, a newer version or a raw file never is (a deliberate file is not damage).

## Versions and migrations

Most updates need no code. When a change has a meaning the engine can't guess — coins became gems at 10:1, one weapon
became an inventory list, one `u32` became two `u16` — bump the version and add a step from the old one. Steps run
**after** the automatic load, in order (a v1 file runs 1 then 2; a v2 file only 2); each gets the struct and a
read-only view of the file's fields:

```cpp
void progress_from_v2(Progress& p, const nnh::save::OldFields& old) {
    std::uint32_t coins = 0;
    if (old.get(nnh::save::path(3), coins)) p.gems = coins / 10;  // field 3 is retired in v3
    std::uint32_t location = 0;
    if (old.get(nnh::save::path(16), location)) {                 // one u32 split in two
        p.room = static_cast<std::uint16_t>(location >> 16);
        p.tile = static_cast<std::uint16_t>(location & 0xFFFF);
    }
}
NNH_SAVE_TYPE(Progress, "progress", NNH_SAVE_VERSION(3), NNH_SAVE_STEP(1, progress_from_v1),
              NNH_SAVE_STEP(2, progress_from_v2), /* fields */ NNH_SAVE_RETIRED(3, 4, 16));
```

`OldFields` reads the **original bytes**: retired, unknown and dropped values are all there. `has(path)`, `kind(path)`
(`WireKind::None` when absent), `shape(path)`, `count(path)` (items of a sequence, −1 otherwise) and `get(path, out)`:
the value converted by the load rules into a number, bool, enum, `Vec2`, `Color`, reference, `data::String` (cut when
longer), an array or list of those, or a struct with its own save declaration (decoded into `T{}` by its ids).
`get` answers false and leaves `out` unchanged when the field is absent or the value doesn't fit. Paths:
`save::path(4)`, `save::path(4, 2)` (field 2 of struct field 4), `save::path(9).item(3).field(2)`. Lookups walk the
fields of the levels they pass: load-time cost, not for every frame.

**Steps can't fail**: what they can't convert keeps its default (they may log). A step that would lose data (a full
list) decides what to keep. **The other pattern**, for big rewrites: keep the old struct with the old ids and the same
key as its own save type (`ProgressV1` in [progress_history.hpp](../../examples/saves/progress_history.hpp)), load the
old file into it, convert by hand.

**A newer file** (a player went back to an older build) is refused with `Unsupported` and changes nothing; the engine
doesn't stop a later store from replacing it. Ask the player before saving over it (`info(path).version`).

## Slots, summaries and settings

A slot is a path (`slot1.sav`, spec 002's path rules: the game's data folder). `exists(path)` (the file or its backup),
`remove(path)` (the file, its backup and a temp file left by a cut store), `info(path)` (the structured header — key,
version, payload size — read with one small read, no CRC check; the backup's when the main file is unusable).

A **summary** declares a subset of a save type's ids under the same key, and the highest version it reads; it loads
with every check of a full load (the whole file validated, the backup when needed) and runs no step (the report's
`version` says which version wrote the file). A "Load game" screen reads each slot's summary without loading its whole
state into the game:

```cpp
struct SlotSummary { nnh::data::String<24> name; std::uint16_t level = 0; std::uint32_t gems = 0; };
NNH_SAVE_SUMMARY(SlotSummary, "progress", 3, NNH_SAVE_FIELD(name, 1), NNH_SAVE_FIELD(level, 2),
                 NNH_SAVE_FIELD(gems, 12));
nnh::save::load("slot2.sav", summary, g_buffer.bytes, sizeof g_buffer.bytes);   // NotFound: an empty slot
```

**Settings** are another save type in another file (`settings.sav`): volume, language, an `ActionSet`'s bindings
blob. They get versions, migrations and the backup for free.

## Buffers and sizes

`max_size<T>()` is the largest encoding of T (every string, list and array full), a compile-time constant.
`Buffer<T, Headroom>` is exactly that many bytes plus `Headroom` for files written by later versions with more data (a
load needs room for the whole file: `LimitReached` otherwise). Keep it static or in a scene — the 3DS main thread's
stack is small. `encode(value, out, capacity)` and `decode(data, size, value)` work in memory (a save inside the game's
own file, several in one blob); `store`/`load` are built on them. Nothing allocates; the stack a load uses is about
1.5 KB on the 3DS for a three-level save (plan 027 stage 7; about 2.9 KB at 8 levels). The module adds about
22.6 KB of code to a 3DS game that uses it (the codec about 14.6 KB); games without structured saves pay nothing.

## The dump

In Debug and Development builds a value, a buffer or a file can be written as text, one line per field:

```text
save 'progress' v3 (3768 bytes)
name (1) string = "Zo\xC3\xAB Rangers"
level (2) u16 = 300
#3 (retired) u32 = 123456
hero (8) struct
  hp (1) u8 = 40
inventory (9) struct list[64]
  [0]
    kind (1) u16 = 100
    wear (5) f32 = 0.5 [3F000000]
```

`dump(value, scratch, capacity, sink)`, `dump_bytes<T>(data, size, sink)` (names from T), `dump_bytes(data, size,
sink)` (ids only, no schema), `dump_file<T>(path, buffer, capacity, sink)`; `log_lines` is a sink that logs each line.
Strings are quoted with `\"`, `\\` and `\xNN` escapes, floats show their bits, enums their `NNH_DATA_ENUM` names. In the
simulator, `NNH_SAVE_DUMP=1` makes every store also write `<path>.txt`. Release builds have no dump code and no member
names (the calls return `Unsupported`; messages name ids).

## Costs

Synchronous: save at a save point or in a menu, after showing a "Saving…" indicator. A store is one encode (in memory)
plus spec 002's five file operations; a load is three file operations plus one validation and one decode. On the PC
(pc-dev, noisy) the reference save of `examples/saves` (3.8 KB) encodes in 0.01–0.02 ms and decodes in 0.03–0.07 ms;
a store to disk takes about 4 ms, as long as a raw `save::write` of the same bytes. Estimated on an Old 3DS (plan 027's
ARM pre-check): encode ≈ 0.3 ms, decode ≈ 0.65 ms. Old 3DS targets (AC-8; the console measurement is pending, stage 10): encode < 0.5 ms, decode < 1 ms, decode
of a v1 file with two steps < 1.2 ms, store < 150 ms median (none over 300 ms), load < 50 ms, load from the backup
< 100 ms, three slot summaries < 150 ms. SD timings vary between cards.

## Migrating from raw saves

A file written by `nnh::save::write` has no structured header: `load` refuses it with `Unsupported` ("a raw nnh::save
file (read it with save::read)"). Read it once with `nnh::save::read` into the old struct, copy the values into the new
save type and `store` it.

## Misuse

A buffer too small: `LimitReached`, nothing written. A bad path: `InvalidArgument` (no file access). A string member
that isn't UTF-8 on `store`/`encode`: an assert in Debug/Development, `InvalidArgument` in Release (a store never
writes a file its own load would refuse). A view asked for a kind that can't hold the value: false. Everything else is
a build error ([above](#declaring-a-save-type)). Never undefined behaviour: the decoder is fuzzed
(`tests/fuzz/fuzz_saves.cpp`, [security.md](../architecture/security.md)).

## Pitfalls

- **Changing what an id means** breaks every old save silently (values land in the wrong field): retire it instead.
- A struct nested in a class, or a template instance, can't be declared (the macro needs an unqualified name in its
  namespace): make it a namespace-scope struct.
- An enum's names come from `NNH_DATA_ENUM` (spec 014); without them, any value of the underlying type loads.
- The play time and other `u64`s are fine; `long` is not (its size differs between systems).
- A store over a newer save replaces it: ask the player first.
