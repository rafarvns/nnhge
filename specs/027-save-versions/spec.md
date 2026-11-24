# Spec 027: Save data with versions

Status: approved 2026-11-24 (overnight pre-approval by the maintainer: every "Decisions to take" item and open
question as recommended; to review in the morning)
Author: NNHGE maintainers · Date: 2026-11-23
Depends on: [spec 002](../002-core-foundation/spec.md) (`nnh::save`: the `NNHS` container with CRC-32, atomic write
through a temp file and the `.bak` backup; `nnh::fs` paths; `Result`, logs, asserts, tagged memory, profiling) and
[spec 014](../014-data-assets/spec.md) (the field kinds and value types of data assets: `data::String<N>`,
`data::List<T, N>`, `Ref<T>`, enums, nested structs and arrays; decision D29). Works with
[spec 017](../017-input-actions/spec.md)'s saved bindings (an `NNHB` blob can live inside a save) and
[spec 007](../007-assets-and-romfs/spec.md)'s assets (the example ships golden saves) without changing either.
Verified on the console with [spec 013](../013-console-remote/spec.md)'s `nnh run --script`. Implements the ROADMAP
item "Save data with versions" (M7, Features in the spirit of Godot and Unity). Optional for games (constitution XI,
decision D25).

## Problem and goal

Today a game saves by handing `nnh::save::write` the bytes of a plain struct and a 16-bit version number. That is
safe against power loss and damage (spec 002: temp file, backup, CRC-32), but the bytes are the compiler's struct
layout: adding a field to `Progress` in an update makes every player's save unreadable unless the game keeps the old
struct around and converts it by hand, a reordered or widened member silently shifts every value after it, and
nothing guarantees that the PC simulator's layout matches the 3DS's (padding, `bool` size, alignment differ between
compilers). Godot (`ConfigFile`, `ResourceSaver`, `store_var`), Unity (`JsonUtility`, `PlayerPrefs`), protobuf,
FlatBuffers and Serde all solve this the same way: values are written **field by field under a stable name or
number**, missing fields take their defaults, unknown ones are skipped, and a version number drives explicit
conversions for what can't be automatic. After this spec an NNHGE game **describes its save struct once in C++**,
giving each field a **stable numeric id**, and stores and loads it with one call; fields can be added, removed,
renamed, reordered and widened between releases without code; a **version number** and ordered **migration
functions** (v1 → v2 → v3) cover the rest. The file layout is fixed by the engine (little-endian, no padding), so the
same save loads on PC and 3DS, and it keeps spec 002's checksum, atomic write and backup. Loading never crashes on a
damaged or edited file, never allocates, and reports what it adjusted. A game that doesn't use it pays nothing, and
raw `nnh::save` keeps working unchanged.

## User stories

- As a game developer, I want to write `NNH_SAVE_*`-style declarations next to my `Progress` struct, giving each
  field an id, and call one function to save it and one to load it, so that I stop copying struct bytes.
- As a game developer shipping an update, I want a field I add to take its default when an old save is loaded, a
  field I remove to be ignored, and a field I rename or move to keep its value, so that players never lose progress
  because I edited a struct.
- As a game developer, I want to bump my save version and write a small function "from version 1" (coins became
  gems at 10:1, a single `weapon` became an inventory list) that runs only for older saves, in order, so that the
  changes automation can't guess are still one function each.
- As a game developer, I want that function to read values the struct no longer has (a retired field), so that the
  conversion doesn't need the old struct definition.
- As a player, I want a save made on the console to load in the simulator and the other way round, and a save made
  before an update to load after it.
- As a player, I want a power cut while saving to leave me with the new save or the previous one, never with
  nothing.
- As a game developer, I want a damaged, truncated or hand-edited save to load as a clear error (or from the backup)
  and never crash my game, and I want to know when a value was adjusted (a name cut to fit, a list shortened).
- As a game developer, I want a "Load game" screen that shows each slot's level and play time without loading every
  slot's full state into my game.
- As a game developer, I want my settings (volume, language, controls) in their own small file with the same
  guarantees, separate from my game slots.
- As a game developer, I want to see a save as readable text in the simulator (field names and values), so that I can
  debug a player's report or check what my game wrote.
- As a game developer, I want to know how long a save takes on an Old 3DS's SD card, so that I place saves where a
  short hitch doesn't hurt.
- As a game developer who saves raw structs, or doesn't save at all, I want my game, its size and its build time to
  stay as they are.
- As a contributor, I want the encoder, decoder and migration rules unit-tested, fuzzed and checked against golden
  files without a console.

## Functional requirements

### Describing a save

- **FR-1 Save schemas in C++.** A game declares which members of a plain struct (an aggregate with default member
  initializers, as spec 014's data types) are saved, through a small family of declarations in ordinary game sources
  — no build step, no `DATA_SCHEMA`, no generated code. A **save type** (a struct that can be a file's root) has a
  **type key** (1–16 characters of `a-z 0-9 _`, as spec 014's keys) and a **current version** (a positive 32-bit
  number, default 1); a nested-only **save struct** has neither. Each saved field names its member and its **id**.
  Mistakes C++ can see are compile errors (an unknown member, an unsupported member type, an id used twice in one
  struct, an id of 0 or out of range, an id listed as retired, a nested struct without its own declaration, too many
  fields, nesting too deep). One struct may be both a spec 014 data type and a save type; the two declarations don't
  interact.
- **FR-2 Stable field ids.** A field is identified in files only by its id (a number from 1 to 65 535, unique within
  its struct; each nested struct has its own id space, like a protobuf message). Renaming a member, moving it in the
  struct or in the declaration, or adding and removing other fields never changes what an id means. A struct may
  declare **retired ids** (fields removed in earlier versions): reusing one is a compile error, and a retired field
  found in a file is skipped without counting as unknown (FR-10) — protobuf's `reserved`.
- **FR-3 Field kinds.** Saved fields may be any kind spec 014 supports — `bool`, 8/16/32-bit integers, `float`,
  `data::String<N>`, enums (stored by their integer value), `Vec2`, `Color`, nested structs, fixed arrays `T[N]`,
  `data::List<T, N>`, `data::Ref<T>`, `TextureRef`, `SoundRef`, `MusicRef` (stored as their 32-bit path hashes, so they
  stay valid across builds as long as the asset's path) — plus **64-bit integers and `double`** (play time, seeds,
  large scores; see Decisions). A list of `std::uint8_t` holds an opaque blob (for example spec 017's `NNHB` bindings
  from `save_to`). Pointers, references, `std::string` and non-trivially-copyable members stay unsupported (a compile
  error). Using these kinds doesn't require the game to use data assets (FR-20).

### The file

- **FR-4 Layout independent of the compiler.** The encoded form is defined by the engine, not by the struct's memory
  layout: little-endian, no padding, floats and doubles as their IEEE bits, `bool` as one byte 0/1, strings as a
  length and UTF-8 bytes, lists as a count and items, fields of a struct in **ascending id** order. Encoding the same
  values with the same schema gives the **same bytes** on PC (every supported host compiler, 32 or 64-bit) and on the
  3DS; a file written on one loads on the other with identical values.
- **FR-5 Self-describing fields.** Each encoded field carries its id and a **wire kind** with enough size information
  to be skipped or checked without the schema (as protobuf's wire types), so a reader can skip unknown or retired
  fields, detect a field whose kind changed, and dump a file to text (FR-17) knowing only ids. The overhead is small:
  the plan states it per field and the reference save's total (target: under 25 % over the raw values).
- **FR-6 Container and integrity.** A structured save is stored through spec 002's `nnh::save` container (magic
  `NNHS`, CRC-32 over the payload, size) and keeps its write procedure — temp file, previous file kept as `.bak`,
  rename — so a power cut at any point leaves the new file or the previous one loadable. Inside the payload a short
  structured header records the engine's structured-format version, the save type's **key hash** and the **game's save
  version** that wrote it. A file of another save type (a `settings` file loaded as `progress`) is refused with a
  message naming both keys. The whole file is written by **one** platform write (header and payload together, no
  write per field: each SD write on the 3DS costs ~20 ms), and a store performs no more file operations than
  `nnh::save::write` does today (counted on the fake platform).
- **FR-7 Backup on structural damage.** On load, a file whose CRC is valid but whose structure is invalid (FR-14) is
  treated like a damaged file: the backup is tried, and the load reports which file was used. If both are unusable
  the load fails with `Corrupted` and changes nothing.
- **FR-8 Caller buffers, compile-time sizes, no allocation.** Encoding and decoding work in a buffer the caller
  provides; the engine gives each save type's **maximum encoded size** as a compile-time constant (every string full,
  every list full), so a game can size a static buffer exactly. Store, load, encode, decode, migrations and dumps
  allocate nothing and call no `virtual` function; their own stack use stays small and is stated by the plan (target:
  under 1 KiB besides the game's struct and buffer — the 3DS main thread's stack is small). Separate calls encode a
  value into a buffer and decode one from a buffer, for games that embed a save in their own file or keep several in
  one (the file calls are built on them).

### Loading, evolution and versions

- **FR-9 Load semantics.** Loading a save type reads the file, **validates the whole file first** (FR-14) and only
  then writes the struct: it becomes `T{}` (the struct's defaults), then every field found in the file with a known id
  is set, then the migrations run (FR-12). On any error the struct is **unchanged**. A missing file (and backup) is
  `NotFound` — start a new game.
- **FR-10 Automatic evolution.** Without any migration code:
  - a field in the struct but not in the file keeps its default (added field);
  - a field in the file whose id the struct doesn't declare is skipped and counted as **unknown** (removed field,
    unless retired: FR-2);
  - renamed or reordered members load by id;
  - integers convert between widths and signedness when the value fits, integers convert to `float`/`double` when
    exact, `float` widens to `double`; an array or list whose capacity grew loads every item, a string whose capacity
    grew loads whole;
  - a nested struct loads by its own ids, recursively, by the same rules.
- **FR-11 Values that don't fit.** When a file value can't be represented in the current field — a number out of the
  new type's range, a kind that changed without an exact conversion (an integer field that became a struct), a
  non-finite float, an enum value outside the enum's declared names (when the enum has spec 014's name list), a
  `bool` byte other than 0 or 1 — the field keeps its default and is counted as **dropped**. A string longer than the
  new capacity is cut at a UTF-8 character boundary, and a list or array with more items than the new capacity keeps
  its first items (an array with fewer items keeps the defaults for the rest); both are counted as **adjusted**.
  Nothing is clamped silently. The migration functions (FR-12) can still read the original value.
- **FR-12 Versions and migrations.** A save type's declaration may list **migration steps**, each a plain function
  pointer (no captures) registered for a **from** version: a step for version *n* converts a value of version *n* into
  version *n + 1*. Loading a file written with save version *v* lower than the current version *c* runs the steps
  *v*, *v + 1*, … *c − 1* **in order** after the automatic load, each receiving the struct (mutable) and a read-only
  **view of the file's fields** (FR-13). A missing step in the chain is a load-time error the developer sees in
  Debug/Development (assert) and a no-op step in Release, never undefined behaviour; a type whose version is above 1
  without a step for version 1 is diagnosed by a unit-test helper the docs show. Steps cannot fail: what they can't
  convert keeps its default (documented; see Decisions). A file with the current version runs no step.
- **FR-13 Reading old fields.** A migration step's view answers, for a field path of ids (`5`, or `4.2` inside nested
  struct 4, or item *i* of list 7), whether the file has it, its wire kind, and its value converted to a requested C++
  kind under FR-10/FR-11's rules (an integer, a float, a string into a buffer, a nested struct into a save struct
  declared for that shape). It reads the original file bytes, so retired, unknown and dropped fields are all
  reachable. The same file can also be loaded into an **older struct type** declared with the old ids (the "keep the
  v1 struct" pattern of Serde-style version chains): the docs show both patterns.
- **FR-14 Untrusted input.** A save file is external data (constitution X): the player can edit it, the SD card can
  damage it. The decoder is bounded — it checks every length, count, id, wire kind, nesting depth (≤ 8) and string
  (valid UTF-8, no length beyond the field's bytes) before use, never trusts a count from the file, runs in time
  linear in the file size, and fails with `Corrupted` and a reason (`progress.sav: field 7: list count 900 exceeds the
  file`) instead of crashing. A CRC that a player recomputed after editing values gives a structurally valid file; its
  values are subject to FR-11 only (no anti-tamper; Non-goals). A file written by a **newer** save version than the
  game's current one is refused with `Unsupported` ("saved by version 5; this game reads up to 3") and changes
  nothing. A file written by raw `nnh::save::write` (no structured header) is refused with its own message, so a game
  migrating from raw saves can read it once with `nnh::save::read` and store it in the new form.
- **FR-15 Load report.** A successful load returns a report: whether the backup was used, the file's save version, the
  number of migration steps run, and the counts of unknown, dropped and adjusted fields (FR-10, FR-11), with the first
  few field paths of each in Debug/Development. Loads that adjusted, dropped or used the backup log one warning line.

### Slots, settings and tools

- **FR-16 Slots, summaries and settings.** A slot is a file path (`slot1.sav`, `slot2.sav`; spec 002's path rules).
  The engine provides: whether a save exists (file or backup), deleting a save (the file, its backup and any temp file
  left by an interrupted write), and reading a file's structured header (type key hash, save version, size) without
  decoding it. A **partial load** — decoding a file into a smaller struct that declares only some of the save type's
  ids, under the same type key — gives a "Load game" screen each slot's summary (level, play time, player name) with
  the same code paths and guarantees; the docs show it. Settings are just another save type in another file (the docs
  show a `settings` type beside `progress` slots).
- **FR-17 Text dump in Debug.** In Debug and Development builds, a value, a buffer or a save file can be written as
  readable text: one line per field with its member name (when the schema is known), id, kind and value, nested
  structs indented, lists with their count, strings quoted; unknown fields by id and wire kind. In the simulator an
  environment variable makes every store also write `<path>.txt` beside the save. Release builds contain no dump code
  and no field names unless the plan shows they cost nothing (the release-strings check must pass).
- **FR-18 Same behaviour everywhere.** Encoding, decoding, conversions, migrations order and reports are pure core code
  with fixed rules: the same file gives the same struct bytes, report and log lines on PC and 3DS; the same values
  give the same file bytes (FR-4).

### Cost, misuse, opt-in

- **FR-19 Bounded cost, measured.** Encoding and decoding are linear in the encoded size with no per-field file
  access, lookups of fields by id within a struct are not quadratic for the documented field limit, and store and load
  add at most the encode/decode time to spec 002's file operations. Costs on an Old 3DS are measured (AC-8). Saving is
  synchronous; the docs explain where to call it (a save point or menu, after showing a "Saving…" indicator) and the
  measured cost.
- **FR-20 Optional and light.** Structured saves are an optional module (XI) with one public header that doesn't
  include graphics, audio, tilemap or entity headers, and doesn't make a game link data-asset loading or the data
  index (sharing spec 014's value types and kind rules must not pull them in). `nnh::save::read`/`write` and every
  existing user of them (spec 017's bindings, NNH Link's `link.sav`) behave and encode exactly as before.
  `examples/hello` keeps its executable size on PC and 3DS.
- **FR-21 Misuse is loud in Debug, harmless in Release.** A buffer too small for an encode (`LimitReached`, nothing
  written), a store of a type without a save declaration (compile error), a missing migration step (FR-12), a
  migration view asked for a kind that can't hold the value (answers "absent"), a path the file layer refuses
  (`InvalidArgument`): errors returned or asserts in Debug/Development, never undefined behaviour.

## Acceptance criteria

- **AC-1 Unit tests first** (constitution VI), on the fake platform: compile-time rules (a negative-compile test list
  in the plan for duplicate ids, retired ids, unsupported members); encoding of every kind and shape against
  hand-written expected bytes (endianness, no padding, ascending ids, 64-bit kinds, strings, lists, nested arrays of
  structs); round trips; each FR-10 evolution rule (added, removed, retired, renamed, reordered, widened, narrowed with
  a fitting value) and each FR-11 rule (out of range, kind changed, non-finite, enum outside names, bad `bool`, string
  cut at a character boundary, list and array shrunk), with the report counts; migrations (chains 1→3 and 2→3, order,
  the old-fields view on retired, unknown and dropped fields, nested paths, a missing step asserting); type key
  mismatch, newer version, raw `nnh::save` file; the struct unchanged on every error; backup fallback for a CRC-valid
  but structurally invalid file; partial loads; delete removing file, backup and temp; dump text; the number of file
  operations per store and load. The Linux build VM runs them with GCC 13/14 and clang 18 and with ASan/UBSan, as
  previous specs did.
- **AC-2 Golden files across versions.** `tests/data/saves/` holds files written by a "version 1" schema of a
  reference save type (about 4 KB encoded: nested structs, an inventory list of 64 item structs, a 1024-entry array of
  explored-room flags, two strings, a 64-bit play time, a `Ref<T>`) and by its "version 2" and "version 3" schemas
  (added, retired, renamed, widened fields, a list that became longer, a value split by a migration). Tests check that
  the v3 code loads each golden file to exactly the expected struct (a checksum of its bytes, equal on every host
  compiler), that re-encoding the current values gives the committed v3 file byte for byte, and that each golden file
  dumps to its committed text. Golden files are only regenerated with a reason written in the commit.
- **AC-3 Power-cut and damage scenarios.** On the fake platform, a store is interrupted after each of its file
  operations in turn (the write cut short, each rename not done) and on a later load the result is always the new save
  or the previous one, never `Corrupted` and never `NotFound` once a save existed; damaged main files (bit flips,
  truncation, a recomputed CRC over broken structure) load from the backup with the report saying so; both damaged
  gives `Corrupted` with the struct unchanged.
- **AC-4 Fuzzing.** The structured decoder (header, every field, the migration view) has a fuzz target that decodes
  into a reference type covering every kind, as given and with the `NNHS` CRC and size fixed so mutations reach the
  structure, then re-encodes every accepted file and checks that it decodes to the same struct; it runs as a
  deterministic mini-fuzz inside `nnh_tests` on every compiler and as a libFuzzer executable on the Linux build VM
  (coverage reported growing, no crash), documented in `docs/architecture/security.md` like the other parsers.
- **AC-5 Example `examples/saves`** (original shapes and the built-in font only), modes chosen by `SAVES_DEMO`
  (`showcase` (default) | `check` | `bench`): a tiny game on the top screen (move, collect coins, pick up items, rename
  the player from a fixed list) whose state is a versioned save type; the bottom screen shows three slots, each with
  its partial-load summary (level, coins, play time) or "empty", and a settings panel (volume, language) saved in its
  own file; buttons and **touch** pick a slot to save, load or delete. The HUD shows the last load report (backup
  used, version, migrated steps, unknown/dropped/adjusted). PC screenshots for empty slots, three saved slots, a load
  from a v1 golden file with its migration report, and a load from the backup after the example damages a file, checked
  by reading the PNGs. In the simulator, the dump environment variable writes `slot1.sav.txt`.
- **AC-6 Console script and cross-platform files.** The `check` mode copies the golden v1, v2 and v3 files from the
  example's assets into the data folder (the SD card on the 3DS), loads each and logs `saves: v1 sum <checksum>
  migrated 2` (and v2, v3), encodes the reference values and logs `saves: encode crc <crc32>`, stores and reloads a
  slot, damages a stored file on the SD card and logs the backup load, then logs `saves: PASS` or `saves: FAIL ...`,
  against golden values recorded from the simulator. `examples/saves/tests/check.nnhs` expects every line and takes
  screenshots of the slots screen. On an Old 3DS the maintainer only opens the Homebrew Launcher and presses Y, and
  `nnh run saves --script examples/saves/tests/check.nnhs` passes; the same script passes with `--pc`. A slot saved by
  the console and copied by hand into the simulator's data folder loads with the same summary (documented as an
  optional manual check; the byte equality is already proven by the encode CRC line).
- **AC-7 Opt-in and no regressions.** `examples/hello` builds with the same executable size as before on PC and 3DS;
  spec 002's save tests, spec 017's bindings tests and fuzz targets, and every earlier spec's tests pass unchanged; the
  bytes `nnh::save::write` produces for a raw blob are unchanged (pinned by a test). A game using structured saves links
  no data-asset loading code (checked on the 3DS build's map file or symbol list). Building a game `.cpp` that declares
  a save type stays within constitution III's budgets (measured before and after on `examples/saves`).
- **AC-8 Cost on the console.** The `bench` mode, measured on an Old 3DS with the profiler and logged as
  `saves bench: PASS <numbers>` (or FAIL), on AC-2's reference save (~4 KB). **Targets** (the plan confirms them
  before code; they predate any console measurement):
  - encode the reference save into a buffer: **under 0.5 ms**; validate and decode it: **under 1 ms**; decode a v1
    file with two migration steps: **under 1.2 ms** (averages over 100 runs);
  - store to the SD card (encode + spec 002's atomic write): **under 150 ms** median over 10 stores, none over
    **300 ms**; load from the SD card: **under 50 ms** median; load falling back to the backup: **under 100 ms**;
  - a partial load of three slot summaries for a "Load game" screen: **under 150 ms** in all.
  The bench also logs spec 002's raw `nnh::save::write` of the same byte count, so the structured layer's share is
  visible. If a target is missed, the implementation is optimised first (fewer file operations within FR-6's rules,
  then the codec); if it is still missed, the budget is set from the first console measurement with margin and
  written in the spec and the docs with the measured numbers (the maintainer's policy for benches, as in specs 019,
  020 and 021). SD timings vary between cards: the docs give the measured card's numbers as an example, not a promise.
- **AC-9 Docs.** A new `docs/api/` page for structured saves (declaring a save type, ids and retired ids, kinds,
  the file and its guarantees, loading rules and the report, versions and migrations with both patterns, slots and
  summaries, settings, buffers and sizes, the dump, costs, limits, what never to change: an id's meaning);
  `docs/api/fs.md` links it from "Saves" and keeps documenting raw saves; `docs/api/data.md` notes that its value types
  serve saves too; `docs/architecture/security.md` lists the new parser and fuzz target; decision rows in
  `docs/architecture/decisions.md` (D45, D46 if the plan takes them); CLAUDE.md layout and ROADMAP updated;
  `docs/REFERENCES.md` citing the prior art below.

## Non-goals

- **Saving engine objects automatically** (worlds, entities, prefabs, tilemaps, tweens, scenes): the game copies what
  it wants to keep into its own save struct. Serialising a spec 020 world needs its own spec (handles, kinds, hooks).
- **Text or human-editable save and settings formats on the console** (Godot's `ConfigFile` INI, Unity's JSON): the
  console never parses text (D29's rule); the dump (FR-17) is one-way. Importing a text file back into a save (for
  tests or modding) is a possible follow-up, host-side only.
- **Encryption, signing or anti-cheat**: a homebrew save on an SD card can't be protected from its owner; CRC-32
  detects damage, not intent. Edited values only go through FR-11's checks.
- **Background or incremental saving** (a thread, or a save spread over several frames): saves are synchronous and
  measured (FR-19, see Decisions).
- **Directory listing, cloud saves, 3DS system save data or extdata, Switch save mounts**: slots are paths in the
  game's data folder (spec 002); other systems' save storage belongs to their platform work, behind the same API.
- **Automatic conversion of raw `nnh::save` files**: the game reads a raw file once with `nnh::save::read` and stores
  it in the new form (FR-14 makes the case recognisable).
- **Schema checks at build time**, a schema file, or code generation: the schema is the compiled C++ declaration, read
  at run time (D29's tables are compiled into the game; no host program is needed for saves).
- **Per-field `min`/`max` rules or `required` fields for saves**: game code validates gameplay values after loading;
  required fields make old saves unloadable, which is what this spec avoids.
- **Compression**: saves of a few KB don't need it; a follow-up if a game's save grows past the SD's comfortable size.

## Constraints

- **I** — portable core code: the codec, conversions and migrations have no platform part; file access stays in spec
  002's `nnh::save` and the platform contract; the same file loads the same on PC and 3DS (FR-4, FR-18).
- **II** — no allocation; the buffer is the game's, sized by a compile-time constant (FR-8); small stack use on the 3DS
  main thread; costs measured on an Old 3DS (AC-8).
- **III** — the declarations are templates and constant tables like spec 014's; one light public header; a game's
  save declarations can live in one source file so other files only see the struct; `hello` unaffected; build times
  measured (AC-7).
- **IV** — no events: storing and loading are calls with `Result`s; a game that wants a `GameSaved` event publishes its
  own.
- **V** — no exceptions (`Result` with reasons), no RTTI (spec 014's address-per-type ids), no `virtual` (function
  pointers for migrations), no heap anywhere in the module.
- **VI** — pure logic tested first on the fake platform, golden files, fuzzing (AC-1 to AC-4); visible behaviour by
  screenshots and on the console (AC-5, AC-6).
- **VII** — encoder, decoder, conversions, migration view, dump and file calls in small separate files; one public
  header.
- **VIII** — header comments, a `docs/api/` page, `examples/saves`, fs/data/security pages updated (AC-9).
- **IX** — written from the published designs below; no code copied; credited in `docs/REFERENCES.md`.
- **X** — the decoder is a bounded parser of external data, fuzz-tested (FR-14, AC-4); saves stay atomic with a
  backup (FR-6, AC-3); writes stay inside the game's folder (spec 002 paths).
- **XI, D25** — optional (FR-20): raw saves stay supported, data assets aren't required, and nothing else depends on
  this module.
- **Under pressure:**
  - **D29 / spec 014's kinds.** Reusing spec 014's field kinds and value types (`String`, `List`, `Ref`) for saves
    makes them shared vocabulary; FR-3 adds 64-bit integers and `double`, which spec 014 refuses. The plan must add
    them without changing data-asset behaviour, fingerprints or error messages (spec 014's `FieldKind` is append-only),
    and must keep the shared deduction code free of the data index (FR-20).
  - **X, "saves written atomically".** Spec 002's temp + rename + backup is atomic at the level the FAT file system on
    the SD card allows; AC-3 proves it on the fake platform by cutting every step, but a real power cut on the 3DS
    can't be automated. A manual "pull the battery during a save loop" test is listed as optional (open question).
  - **Spec 002's `nnh::save`.** FR-7 (try the backup when the structure is invalid) needs spec 002's read path to
    accept a validation step or an internal "read the backup" call; the public raw API and its bytes must not change
    (AC-7).
  - **Release strings.** Field names are needed for the dump and for clear messages; FR-17 keeps them out of Release
    unless the plan shows the release-strings check still passes. Spec 014's tables keep names in Release today.
  - **AC-8's** targets predate any console measurement and fall back to the measurement-with-margin policy; SD card
    timings vary by card.

## Prior art

- **Godot**: `ConfigFile` (sections of named keys, typically for settings) and `ResourceSaver`/`ResourceLoader` for
  saving resources; the "Saving games" tutorial recommends a version-tolerant dictionary per object —
  [ConfigFile](https://docs.godotengine.org/en/stable/classes/class_configfile.html),
  [ResourceSaver](https://docs.godotengine.org/en/stable/classes/class_resourcesaver.html),
  [Saving games](https://docs.godotengine.org/en/stable/tutorials/io/saving_games.html). FR-10's "missing keys keep
  defaults" and FR-16's settings file.
- **Unity**: `JsonUtility` (fields by name; missing fields keep defaults, unknown ones are ignored) and `PlayerPrefs`
  (key/value settings) — [JsonUtility](https://docs.unity3d.com/ScriptReference/JsonUtility.html),
  [PlayerPrefs](https://docs.unity3d.com/ScriptReference/PlayerPrefs.html). We use ids instead of names so renames
  are free and the console parses no text.
- **Protocol Buffers**: field numbers identify fields on the wire, must never change or be reused, removed numbers are
  `reserved`, old data gets defaults for new fields and unknown fields are skipped; wire types make skipping possible —
  [Language Guide (proto 3): assigning field numbers, deleting fields, updating a message type](https://protobuf.dev/programming-guides/proto3/).
  FR-2, FR-5, FR-10.
- **FlatBuffers**: schema evolution by explicit `id` attributes so fields may be declared in any order, and
  `deprecated` instead of removal — [Evolution](https://flatbuffers.dev/evolution/). FR-2's retired ids.
- **Serde** (Rust): `#[serde(default)]` for fields missing from old data, `rename`/`alias` for renames, and the common
  pattern of versioned structs converted in a chain —
  [Field attributes](https://serde.rs/field-attrs.html), [Attributes](https://serde.rs/attributes.html). FR-12's
  ordered steps and FR-13's "load into the old struct" pattern.
- **Butano** (GBA): `bn::sram::read`/`write` copy a trivially copyable struct whole, with a magic field the game
  checks — [bn::sram](https://gvaliente.github.io/butano/namespacebn_1_1sram.html). What NNHGE's raw `nnh::save` does
  today, and what this spec moves beyond.
- **Pokémon Generation III** save layout: two save blocks written alternately, each section with an id, a checksum and
  a save index, the previous save used if a checksum of the newest fails —
  [Bulbapedia: Save data structure (Generation III)](https://bulbapedia.bulbagarden.net/wiki/Save_data_structure_(Generation_III)).
  The idea behind spec 002's `.bak` and FR-7.
- **GBATEK** on GBA cartridge SRAM, for why console saves have always been small, checksummed and written whole —
  [GBA Cart Backup SRAM/FRAM](https://problemkaputt.de/gbatek-gba-cart-backup-sram-fram.htm).

## Decisions to take

Every question has a recommended answer; none is decided until the maintainer approves the spec. If the plan takes
architecture decisions, they are rows **D45** (schema and file format) and **D46** (evolution and migration model).

- [x] **How is a save struct described?** Options: (a) raw struct bytes plus a version (today); (b) add an `.id`
      option to spec 014's `NNH_DATA_FIELD` and reuse `NNH_DATA_TYPE`; (c) **a sibling family of save declarations
      (save type with key and version, save struct, save field with id, retired ids) built on spec 014's kind
      deduction and value types, usable in any game source, with no build step**; (d) a hand-written `serialize(archive&)`
      function per struct (Cereal/Boost style). — **recommended: (c)** (FR-1). (b) would put save types under
      `DATA_SCHEMA`, its host program and its "declarations only" rule, coupling saves to the data pipeline (XI) for no
      benefit (saves need no build check). (d) is flexible but easy to get wrong (read and write orders diverge),
      can't produce a dump with names or a compile-time maximum size, and gives no automatic evolution.
- [x] **How are fields identified in the file?** Options: (a) declaration order plus the version; (b) a hash of the
      member name (as spec 017's `NNHB` keys actions); (c) **explicit numeric ids, unique per struct, with retired ids
      checked at compile time**. — **recommended: (c)** (FR-2). (a) breaks on any reorder; (b) breaks on a rename
      and can collide; ids are what protobuf and FlatBuffers converged on, and a compile-time check makes reuse
      impossible.
- [x] **Wire format.** Options: (a) fixed layout per version (offsets); (b) **self-describing tagged fields (id +
      wire kind + size where needed), little-endian, ascending id, inside spec 002's `NNHS` container with a small
      structured header (format version, type key hash, game save version)**; (c) a general format (CBOR, MessagePack,
      protobuf proper). — **recommended: (b)** (FR-4 to FR-6). (a) can't skip unknown fields; (c) brings a dependency
      or a larger parser for features saves don't need; (b) keeps the atomic write, backup and CRC already tested and
      fuzzed, and lets the dump and a future host tool read files without the game.
- [x] **What does the CRC-valid but malformed case do?** — **recommended: try the backup, as for a CRC failure, and
      report it** (FR-7). A file is useless if it can't be decoded, whatever its checksum says.
- [x] **Versions and migrations.** Options: (a) automatic evolution by ids only; (b) only a version number and
      game-written conversions of whole old structs; (c) **automatic evolution by ids plus a game save version and
      ordered migration steps (from *n* to *n + 1*), each given the struct and a read-only view of the file's fields;
      loading into an older struct type stays possible as a pattern**. — **recommended: (c)** (FR-10 to FR-13). Most
      updates need no code; the version handles semantic changes (units, splits, merges); the field view avoids keeping
      every old struct alive, and the old-struct pattern remains for big rewrites.
- [x] **Can a migration step fail?** Options: (a) **no: steps are infallible; anything they can't convert keeps its
      default, and they may log**; (b) steps return a `Result` and a failure fails the load. — **recommended: (a)**
      (FR-12). (b) would need a second copy of the struct to keep "unchanged on error" (FR-9; a 16 KB struct on the 3DS
      stack), and a player with a save the game refuses is worse off than with one partly reset. Revisit if a game
      needs to refuse.
- [x] **Values that don't fit after a schema change.** Options: (a) refuse the whole file; (b) clamp numbers and
      truncate everything; (c) **exact conversions only for numbers (else the default, counted as dropped); strings cut
      at a character boundary and lists/arrays shortened, counted as adjusted; everything reported and logged once**.
      — **recommended: (c)** (FR-11, FR-15). (a) loses a player's progress for one field; (b) silently changes meaning
      (a clamped `u8` level of 255); a cut name is still the player's name.
- [x] **Files from a newer game version.** Options: (a) **refuse with `Unsupported`, change nothing; the engine does
      not stop a later store from replacing the file**; (b) also refuse to store over a newer file unless forced; (c)
      load it, skipping unknown fields. — **recommended: (a)** (FR-14), with the docs telling games to ask the player
      before saving over it. (b) costs an extra SD read (~20 ms) on every store; (c) can lose the newer fields at the
      next store without the player knowing. See the open question.
- [x] **Which kinds?** — **recommended: spec 014's kinds plus 64-bit integers and `double`; references stored as
      path hashes; blobs as `List<std::uint8_t, N>`** (FR-3). Play time in frames passes 2³² after 2.2 years only, but
      millisecond timers, seeds and large scores need 64 bits; the kinds are append-only, so data assets don't change.
- [x] **Buffers and memory.** Options: (a) **caller buffers with a compile-time maximum encoded size per save type**;
      (b) an engine scratch block allocated at the first save; (c) the stack. — **recommended: (a)** (FR-8). No
      allocation at all, the size is known before shipping, and games that save rarely don't keep a block alive; (c)
      overflows the 3DS main thread's stack for large saves.
- [x] **Missing file.** — **recommended: `NotFound` (no file and no backup), struct unchanged**, as spec 002's
      `save::read` (FR-9). A new game is the normal answer.
- [x] **Slots and "Load game" summaries.** Options: (a) **a path per slot, existence/delete/header helpers, and partial
      loads into a summary struct that declares a subset of the ids**; (b) a separate summary file per slot written
      alongside; (c) a slot manager object with a fixed slot count. — **recommended: (a)** (FR-16). It reuses the
      decoder with no second file to keep in sync (a power cut between two files could make them disagree); (c) imposes
      a structure on games (XI). The cost of reading whole files for summaries is measured (AC-8).
- [x] **Settings vs game saves.** Options: (a) **the same mechanism, a separate save type in its own file**; (b) a
      key/value settings store (`PlayerPrefs`-like); (c) a text settings file. — **recommended: (a)** (FR-16).
      Settings get versions, migrations and the backup for free; (b) adds a second API for the same job; (c) needs a
      text parser on the console (Non-goals).
- [x] **Debug dump.** — **recommended: a text dump of values, buffers and files in Debug/Development, names when the
      schema is known, ids otherwise; an environment variable in the simulator writes `<path>.txt` on every store; no
      import back; a host `nnh` subcommand later** (FR-17). Players' bug reports can be read by the developer's Debug
      build; the self-describing format keeps a host tool possible without the game.
- [x] **Limits.** — **recommended: ids 1–65 535; ≤ 128 saved fields per struct; nesting ≤ 8 levels; a root struct
      ≤ 64 KiB in memory; arrays of numbers up to 65 535 elements (an explored-rooms grid), lists as spec 014
      (≤ 256 items), strings ≤ 255 bytes; an encoded save ≤ 256 KiB** (well under spec 002's 16 MB). Spec 014's
      16 KiB per struct and 64 fields are too small for whole game saves; these are still bounded for the fuzzer and
      the compile-time size. The plan confirms them.
- [x] **Synchronous or background saves?** Options: (a) **synchronous, measured, documented**; (b) a worker thread on
      the 3DS; (c) a save split across frames. — **recommended: (a)** (FR-19, Non-goals). A short hitch at a save
      point is normal in console games; threads bring locking into a single-threaded engine. Revisit if AC-8 measures
      stores above 300 ms.
- [x] **Integrity strength.** — **recommended: keep spec 002's CRC-32, no encryption or signature** (Non-goals).
      Damage detection is the goal; anyone can recompute any checksum on an SD card.
- [x] **Raw saves written before this spec.** — **recommended: recognised and refused with their own message, read
      by the game through `nnh::save::read` once** (FR-14). Guessing the struct of a raw blob is impossible.
- [x] **Module and header.** — **recommended: one optional module and public header; spec 014's value types and kind
      deduction shared through a light detail header so saves don't link data-asset loading; `nnh::save`'s public
      API unchanged** (FR-20). Names (header, declaration macros, store/load/encode/decode calls, the report type —
      `nnh::LoadReport` is already spec 017's) are fixed by the plan.
- [x] **Example.** — **recommended: a new `examples/saves` (slots, settings, golden v1/v2/v3 files in its assets and
      in `tests/data/saves/`, check and bench modes)** (AC-5, AC-6). No existing example has a save worth versioning;
      the platformer could adopt it later for a "continue" feature.
- [x] **Old 3DS cost targets** — **recommended: AC-8's targets, confirmed by the plan, and set from the first console
      measurement with margin if they are still missed after optimisation** (the maintainer's bench policy).

## Open questions

- [ ] Should a store refuse to replace a file written by a newer save version (an extra header read per store), or is
      `Unsupported` on load plus documentation enough? — recommended: documentation only (Decisions, "Files from a newer
      game version").
- [ ] Should the maintainer run an optional real power-cut test (remove the battery or the SD card during a loop of
      stores on the Old 3DS, then load)? — recommended: optional, once, recorded in the spec if done; AC-3 is the
      required proof.
- [ ] Field names in Release builds: spec 014's tables keep them; should save tables strip them in Release (messages
      by id only)? — recommended: strip in Release if the shared tables allow it cheaply; otherwise keep them and note
      the release-strings check's scope.
- [ ] Should spec 014 data types also gain 64-bit integers and `double` now that the kinds exist? — recommended: no,
      not in this spec (data files have no need yet, and the TOML reader would change); a later spec can open them.
