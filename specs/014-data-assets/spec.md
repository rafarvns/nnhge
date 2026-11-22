# Spec 014: Data assets

Status: approved (2026-11-22, maintainer pre-approved recommendations: every open question below is answered with
the recommended answer); implemented (console check pending; Linux fuzz/probe pending) — see the [tasks](tasks.md)
Author: NNHGE maintainers · Date: 2026-11-22
Depends on: [spec 007](../007-assets-and-romfs/spec.md) (assets folder, paths, build-time conversion),
[spec 002](../002-core-foundation/spec.md) (`Result`, memory tags, logs, `nnh::fs`), decision D27 (`nnh-assetc`, the
host converter, first used by [spec 012](../012-tilemaps-and-collision/spec.md)). Implements the first item of the
ROADMAP's "Features in the spirit of Godot and Unity". Optional for games (constitution XI, decision D25).

## Problem and goal

Today a game that wants its enemies' stats, its items or a level's settings outside its code has two choices:
hard-code them (every balance change is a recompile) or read a raw text file with `assets::load` and write its own
parser, its own error messages and its own checks, and find typos only when the game misbehaves on the console. Unity
solves this with ScriptableObjects and Godot with Resources: typed data in its own file, shared by everything that
uses it, edited without touching code. After this spec a game declares a plain C++ struct (`Enemy`, `Item`) and, next
to it, a short table naming its fields once; it writes one small, hand-readable text file per enemy or item in its
assets folder; the **build** checks every file against that struct (unknown field, wrong type, out of range, a missing
referenced item or texture) and stops with `file:line: reason`; the game loads a file by path into its own struct
with one call, with no parsing and no allocation of its own, the same on PC and 3DS. Changing a value means editing
a text file and rebuilding the assets — no game code is recompiled. A game that doesn't want any of this ignores it
and pays nothing.

## User stories

- As a game developer, I want to describe an enemy, an item or a level's settings as a plain C++ struct and keep the
  values in their own text files, so that tuning the game doesn't mean recompiling it.
- As a game developer, I want to write those files by hand in any text editor, with comments, so that I don't need
  an editor or a tool to change a number.
- As a game developer, I want the build to tell me `enemies/goblin.enemy:7: unknown field 'damgae' (did you mean
  'damage'?)`, so that a typo never reaches the console.
- As a game developer, I want an enemy's file to say its drop is `items/potion` and its sprite is `art/goblin`, and the
  build to check that both exist and are the right kind, so that broken references fail early.
- As a game developer, I want to load a data asset by path into my struct and follow its references when I need
  them, without the engine allocating behind my back while the game runs.
- As a game developer, I want to list every asset of a type (every item in the game) so that a shop or a bestiary
  doesn't need a hand-written list of files.
- As a game developer who prefers my own formats, I want to ignore data assets entirely and lose nothing.
- As a contributor, I want the data text reader, the converter and the run-time loader tested without a console and
  fuzzed, because data files are external input.

## Functional requirements

### Declaring a data type (the C++ side)

- **FR-1 A data type is a C++ struct plus a schema declared once in C++.** The game writes a plain struct (an
  aggregate: no constructors of its own, no virtual functions, no pointers, no owning members) with default member
  initializers, and declares its **schema** once, in C++, with a small table that names the type's **key** (used as
  the file extension, e.g. `enemy`) and lists the fields that data files may set. The field kind is deduced from the
  member's C++ type at compile time; no RTTI, no exceptions, no generated code the game has to look at. Illustrative
  shape (the plan fixes the names; the public API is the product here):

  ```cpp
  struct Item {
      nnh::data::String<24> name;
      int price = 10;
      nnh::data::TextureRef icon;
  };
  struct Enemy {
      nnh::data::String<24> name;
      int hp = 10;
      float speed = 1.0f;
      Element element = Element::Normal;          // a game enum
      nnh::data::Ref<Item> drop;                  // another data asset
      nnh::data::SoundRef hurt_sound;
  };
  // In one game source file listed as a schema source (FR-3):
  NNH_DATA_ENUM(Element, Normal, Fire, Ice);
  NNH_DATA_TYPE(Item, "item", NNH_DATA_FIELD(name), NNH_DATA_FIELD(price, .min = 0), NNH_DATA_FIELD(icon));
  NNH_DATA_TYPE(Enemy, "enemy", NNH_DATA_FIELD(name), NNH_DATA_FIELD(hp, .min = 1, .max = 9999, .required = true),
                NNH_DATA_FIELD(speed), NNH_DATA_FIELD(element), NNH_DATA_FIELD(drop), NNH_DATA_FIELD(hurt_sound));
  ```

  A field the schema doesn't list is invisible to data files and keeps its default. A schema that names a member
  the struct doesn't have, a member of an unsupported type, or two types with the same key is a **compile error**
  (or a build error naming the schema source), never a run-time surprise.
- **FR-2 Field kinds.** Supported member types: `bool`; signed and unsigned integers of 8, 16 and 32 bits (the text
  value must fit, else an error); `float`; fixed-capacity strings `data::String<N>` (UTF-8, N ≤ 255 bytes); game
  enums declared with their names (written by name in the file); `nnh::Vec2` and `nnh::Color`; nested structs that
  have their own schema (written as a section, FR-6); fixed arrays `T[N]` (exactly N values) and fixed-capacity lists
  `data::List<T, N>` (0..N values, count kept; N ≤ 256) of any kind above; and references (FR-8). Per field, the
  schema may give a numeric `min`/`max` and `required` (the file must set it). Every kind is trivially copyable, so a
  loaded struct can be copied, stored in arrays and saved like any plain struct.
- **FR-3 One declaration, used by the game and by the build.** The schema sources (the files that hold the
  `NNH_DATA_TYPE` tables) are named in the game's build (`nnh_add_game(... DATA_SCHEMA src/data_types.cpp)` or
  equivalent). The build checks data files against **that same declaration** compiled for the host computer — never
  a copy, never a separate schema file, never parsing C++ text. Consequence the docs state: schema sources and the
  headers they include must compile on the host (plain C++ plus `<nnh/...>` headers; they are compiled, not linked
  against the engine). The check does not depend on how the host compiler lays out the struct (FR-10).

### The text format (the file side)

- **FR-4 One file per asset, its type in the extension.** A data asset is a text file in the game's `ASSETS` folder
  named `<name>.<key>`: `enemies/goblin.enemy`, `items/potion.item`, `levels/forest.level`. The extension selects
  the type; a file whose extension is a declared key is a data asset, every other file is untouched (spec 007).
  Its **name** is its path without the extension (`enemies/goblin`) and must be unique among all data assets of the
  game, whatever their type (two files `potion.item` and `potion.enemy` in one folder are a build error).
- **FR-5 A small, strict subset of TOML.** The text is UTF-8 and every valid NNHGE data file is a valid
  [TOML 1.0](https://toml.io/en/v1.0.0) document, so any editor's TOML highlighting works (the docs show how to map
  the extensions), but only this subset is accepted:
  - `field = value` lines; `#` comments to the end of the line; blank lines; LF or CRLF;
  - values: `true`/`false`; decimal integers (optional sign, `_` separators); decimal floats (`1.5`, `-0.25`,
    `1e3`); basic strings in double quotes with the escapes `\" \\ \n \t \uXXXX`; inline arrays `[1, 2, 3]` (may
    span lines, trailing comma allowed);
  - enums as strings (`element = "Fire"`); `Vec2` as `[x, y]`; `Color` as `"#RRGGBB"` or `"#RRGGBBAA"`;
    references as strings holding an asset path without extension (`drop = "items/potion"`, `icon = "art/potion"`),
    or `""` for none.
  - Not accepted (each refused with a clear message): dates and times, literal and multi-line strings, inline tables,
    dotted keys, hexadecimal/octal/binary numbers, `inf`/`nan`, a key set twice.
- **FR-6 Nested structs and lists of structs.** A nested-struct field is written as a TOML table `[field]` followed
  by its own `key = value` lines; a `data::List` or array of structs as repeated TOML array-of-tables `[[field]]`
  blocks (e.g. a loot table: several `[[drops]]` each with `item` and `chance`). Nesting depth is bounded (FR-14).
- **FR-7 Defaults and strictness.** A field the file doesn't set keeps the struct's default member initializer,
  unless the schema marks it `required`. An unknown field, a value of the wrong kind, an integer or float outside its
  type or the schema's `min`/`max`, a string longer than its capacity, too many list items, a wrong number of array
  items, or an unknown enum name is an error. Unknown fields and enum names suggest the closest declared name
  ("did you mean ...?").

### References

- **FR-8 References to other data assets and to media.** `data::Ref<T>` names another data asset of type `T`;
  `data::TextureRef`, `data::SoundRef` and `data::MusicRef` name a texture, a sound or a music asset by the same
  path a game passes to `load_texture`, `load_sound` and `music::play`. The **build** checks that every non-empty
  reference names an existing asset of the right type (a `Ref<Item>` to an enemy is an error; a `TextureRef` must be
  an image the build accepts as a texture). References are small values (they don't hold a path string), can be
  compared with `==`, tested for emptiness, and turned back into their path. Cycles are allowed (an item that
  summons the enemy that drops it), because references are **never followed automatically**: loading an enemy does
  not load its drop, its texture or its sound; the game follows a reference with an explicit load when it wants
  (FR-11), so nothing is loaded or allocated that the game didn't ask for.

### Build-time checking and conversion

- **FR-9 Every data file is checked by the build.** A game build with data assets reads every data file, checks it
  against its type's schema (FR-5 to FR-8, FR-14) and fails on the first file with errors, printing every error of
  that file as `<path relative to the project>:<line>: <reason>` (for example
  `assets/enemies/goblin.enemy:9: drop: no item 'items/potoin' (no file assets/items/potoin.item)`), most useful
  first, at most 20 per file. Editing a data file reruns only that file's step (and the steps of the asset index,
  FR-12), like images and maps today.
- **FR-10 Converted to a compact binary at build time; the text is not shipped.** Each data file becomes one
  converted asset packed with the game (romfs on 3DS, the assets folder on PC), in a documented, versioned binary
  format that does not depend on the host compiler's or the console compiler's struct layout (fields are stored by
  schema field, not as a memory image). The converted asset carries a **fingerprint of its type's schema** (key,
  field names, kinds, capacities, enum names). The text file itself is not packed. The converter verifies what it
  writes with the same bounded validator the console runs (as for maps, D27).
- **FR-11 Loading by path.** `data::load<T>(path, out)` (or the equivalent the plan fixes) loads the asset `path`
  (`"enemies/goblin"`, no extension) into the game's `T`: it starts from `T{}` (the defaults compiled into the game),
  then applies the values from the file. A reference is followed with the same call (`data::load(enemy.drop,
  item)`). Errors are returned as a `Result`, never a crash, and leave `out` unchanged: `NotFound` (no such data
  asset), `InvalidArgument` (bad path, or the asset is of another type), `Corrupted` (damaged file, a fingerprint that doesn't match the game's schema — "converted for an older version of `Enemy`:
  rebuild the assets"), `Unsupported` (before `nnh::init()`). Values loaded are bit-for-bit the same on PC and 3DS
  (floats are parsed once, at build time).
- **FR-12 Listing by type.** The game can count and list the data assets of a type (`data::count<Item>()` and the
  name or reference of the i-th one, in a stable order: by path), so a shop, a bestiary or a level select needs no
  hand-written list. Listing allocates nothing beyond FR-13's index.

### Memory, safety, opt-in

- **FR-13 Memory rules.** Data assets are a load-time feature, like textures (`docs/api/assets.md`): the game owns
  every loaded struct (on its stack, in its scene, in its own arrays); a load allocates at most a bounded temporary
  buffer (engine memory, its own tag `data`) and frees it before returning; the only engine memory kept is the
  game's **data index** (paths and types of its data assets and of the media they reference, to turn references into
  paths and to list by type), loaded on the first data call or by an explicit preload, sized in the docs per entry,
  and released at `nnh::shutdown()` (or explicitly). No data call allocates in a per-frame path once the index is
  loaded; the docs say to load in `on_enter`, not every frame.
- **FR-14 Bounded everywhere.** Documented limits are enforced at build time with a message and again at load time
  with an error: text file size, line length, nesting depth, list capacity, string length, fields per type, struct
  size, types per game, data assets per game (proposed values in the open questions; the plan confirms them against
  the 3DS budget). The text reader and converter, and the run-time validator, are bounded and fuzz-tested
  (constitution X): no input makes them crash, hang, read out of bounds or allocate without a limit.
- **FR-15 Optional and independent.** A game without data assets and without `DATA_SCHEMA` gets no extra build step,
  compiles no host program, links no data code and loads no index. Data assets need no other optional feature
  (tilemaps, tags, prefabs) and no other optional feature needs them; a game can mix them with its own formats and
  read any other file with `assets::load` as today.
- **FR-16 Same behaviour everywhere.** The same data files and schema give the same accepted/refused result on every
  host (Windows, Linux), and the same loaded values in the simulator and on the 3DS.

## Acceptance criteria

- **AC-1 Example `examples/data`** (original art made for NNHGE): a small bestiary/shop. Data files for several
  enemies (stats, an enum element, a sprite, a hurt sound, a list of drops with chances) and items (name, price,
  icon), plus one settings file (`game.settings`: title text, colours, music). The top screen shows the selected
  enemy's sprite and stats; the bottom screen lists every enemy found by `data::count/list` and shows its drops by
  following references. Screenshot on PC (`NNH_EXIT_AFTER_FRAMES` + `NNH_SCREENSHOT`) and a run on an Old 3DS
  (yes/no check: the same values as the simulator).
- **AC-2 Values change without recompiling.** In the example, editing `enemies/goblin.enemy` (its `hp`) and building
  again runs no C++ compile or link of game or engine code (checked from the build log: only the data step and asset
  packing run), stays within the simulator budget (< 2 s, constitution III), repacks the `.3dsx` without compiling,
  and the screenshot shows the new value. The numbers go in the ROADMAP build table.
- **AC-3 Build errors as `file:line: reason`.** Converter tests with bad fixtures, one per rule of FR-5 to FR-8 and
  FR-14 (unknown field with suggestion, wrong kind, out of range, string too long, list too long, unknown enum name,
  missing required field, key set twice, unsupported TOML feature, missing reference, reference of the wrong type,
  texture reference to a non-texture, duplicate data asset name, over each limit), each checking the exact message
  and line; and a build test (ctest) where a game with a broken data file fails with that message.
- **AC-4 Unit tests** (written first, constitution VI): schema declaration (kinds deduced, defaults, min/max,
  required, enums, nested types, lists, arrays); the text reader on good files (every value form, CRLF, comments,
  trailing commas, nested tables, arrays of tables); conversion and loading round trip on the fake platform (every
  kind, defaults kept for unset fields, `out` unchanged on error); references (`==`, empty, to path, following them,
  cycles); listing by type; the fingerprint check (an asset converted for a changed struct is refused); a layout
  test showing the converted bytes are the same whatever the struct layout.
- **AC-5 Bounded and fuzzed.** Fuzz targets for the text reader/converter and for the run-time validator (in the
  mini-fuzz of `nnh_tests` and as libFuzzer targets) run on Linux under ASan/UBSan without findings; a test feeds
  each limit plus one and checks the error.
- **AC-6 Opt-in, nothing to pay.** A game without data assets builds with the same steps and the same executable
  size as before (checked on `examples/hello`, PC and 3DS); `examples/platformer` keeps working untouched; a 3DS
  build of `examples/data` works with a prebuilt `-DNNH_ASSETC=<path>` too.
- **AC-7 Memory.** The leak report is clean after the example quits; a test shows that loads free their temporary
  buffer and that, after the index is loaded, `data::load`/`count`/`list` allocate nothing; the docs give the index
  cost per entry and the example's measured total.
- **AC-8 Docs.** `docs/api/data.md` (declaring a type, the file format with every value form, references, loading,
  listing, errors, limits, memory), the format section in `docs/api/assets.md`, `docs/tech/cmake.md` (the schema
  step, host compilation in cross presets), a decision row in `docs/architecture/decisions.md`, CLAUDE.md layout
  and ROADMAP updated, `docs/REFERENCES.md` citing Unity ScriptableObjects, Godot Resources and TOML.

## Non-goals

- An editor, inspector or GUI of any kind (constitution XI). Files are written by hand.
- Hot reload of data files in a running game: the ROADMAP's later item "Hot reload of assets in the simulator".
- Writing data assets from the game, or saving structs: `nnh::save` today, and the ROADMAP's "Save data with
  versions" later. Data assets are read-only, like every asset.
- Migrating old data files when a struct changes: data files live with the source and are checked by every build
  against the current struct, so there is nothing old to migrate (FR-10's fingerprint only catches stale converted
  assets).
- Inheritance between data files ("this goblin is that goblin with more hp"), expressions, includes or variables in
  the text. A follow-up if a game needs it.
- Polymorphic fields (a reference to "any data asset"), pointers, `std::string`/`std::vector`, heap-owning members.
- Following references automatically, a shared cache of loaded assets, or reference counting (the game owns what it
  loads). Loading textures or sounds named by a data asset (the game calls `load_texture`/`load_sound` with the
  reference).
- Reading the text files at run time on the console, and full TOML (dates, inline tables, dotted keys, literal
  strings).
- Tiled objects or map properties naming a data asset — a natural link to make with prefabs; not in this spec.
- Prefabs, tags, localization tables: separate ROADMAP items, which may later reuse this format.

## Constraints

- **I** — the same calls and the same files on PC and 3DS; no platform code in game code or in the schema; the
  build checks are the same on every host.
- **II** — loaded structs live in game memory; the engine keeps only the index (bounded, sized in the docs); the
  limits (FR-14) keep the worst case within an Old 3DS budget.
- **III** — under pressure: games with data assets compile their schema sources for the host (in cross presets, a
  host build like `nnh-assetc`'s, only when the game declares a schema). Editing a data file must not recompile
  anything (AC-2); editing a schema source recompiles only that file for both targets. The plan measures the first
  build and an incremental one before and after.
- **IV** — no events are needed: loading is a query/command of the data module.
- **V** — no exceptions, no RTTI (kinds deduced by templates at compile time), no `virtual`, no allocation per frame.
- **VI** — the schema, reader, converter and loader are unit-tested first; the example is checked by screenshot
  and on the console.
- **VII** — reader, converter, validator and loader stay in small files; the public header stays light (it is
  included by game headers that declare data structs).
- **VIII** — header comments, `docs/api/data.md` and `examples/data`.
- **IX** — the example's art and sounds are original; the format is our own subset of an open specification (TOML,
  MIT-licensed spec); no code copied from TOML libraries with incompatible licences.
- **X** — data files are external input: the text reader (host) and the binary validator (console) are bounded and
  fuzzed; a damaged converted asset is an error, never a crash.
- **XI, D25** — entirely optional, needs no other feature, and plain C++ (hard-coded values or `assets::load` with the
  game's own parser) stays fully supported.
- **D27** — conversion runs in `nnh-assetc`, which stays generic (no game code inside it), so a prebuilt
  `-DNNH_ASSETC` still works.

## Prior art

- Unity ScriptableObjects: typed data assets defined by a C# class, created and edited in the Inspector
  ([manual](https://docs.unity3d.com/Manual/class-ScriptableObject.html)). NNHGE keeps the idea (a type declared in
  code, many asset files, referenced from elsewhere) without the editor.
- Godot Resources: a script with exported properties; saved as version-control-friendly text `.tres`, exported as
  binary `.res` for speed ([docs](https://docs.godotengine.org/en/stable/tutorials/scripting/resources.html)) — the
  same text-in-repo, binary-in-game split as FR-10.
- TOML 1.0 ([spec](https://toml.io/en/v1.0.0)): the syntax FR-5 subsets, chosen because it is easy to write by hand,
  has comments and is already highlighted by editors.

## Open questions

All answered with the recommended answer, pre-approved by the maintainer (2026-11-22).

- [x] **How does the build know the struct's shape without RTTI or reflection?** Options: (a) a schema table in C++
      (`NNH_DATA_TYPE`/`NNH_DATA_FIELD` macros over member pointers; kinds deduced by templates), compiled into the
      game and, for the build, into a small host program that writes a schema description `nnh-assetc` reads;
      (b) a separate schema text file from which the build generates the C++ struct; (c) a separate schema file plus
      generated `static_assert`s against a hand-written struct; (d) `nnh-assetc` parsing the C++ header as text;
      (e) checking only at run time. — **answer: (a).** The struct and its field list sit side by side in C++, the
      compiler checks every member name and type, defaults come from the struct itself, enums are named in C++, and
      there is one source of truth (FR-3). `nnh-assetc` stays generic and reads the description, so it can still be
      prebuilt (D27). Cost: the schema sources are compiled for the host (constitution III pressure, measured in the
      plan); (b)/(c) mean two places to edit and generated code, (d) is fragile, (e) breaks "the build checks".
- [x] **Text format: TOML-like subset, INI or JSON?** — **answer: a strict subset of TOML 1.0** (FR-5/FR-6). Readable
      by hand, comments, no quotes on keys, no trailing-comma traps, nested sections and lists of sections cover
      loot tables; every file is valid TOML, so editors highlight it. JSON (already parsed by `nnh-assetc` for Tiled)
      has no comments and is noisy by hand; INI has no standard for types, arrays or nesting.
- [x] **Load-time parsing or build-time conversion?** — **answer: build-time conversion to a compact binary**
      (FR-10), validated again at load. The console never parses text (smaller code, no float parsing differences,
      faster loads, a smaller fuzz surface), errors are found by the build, and the format is layout-independent so
      host and console compilers needn't agree on struct layout.
- [x] **How is the type of a file known?** — **answer: the file extension is the type's key** (`goblin.enemy`).
      Listing by type and checking references need no file header, and the type is visible in a file browser.
      Alternative rejected: a `type = "Enemy"` line in every file (easy to forget, one more thing to keep in sync).
- [x] **How are references stored?** — **answer: as a 32-bit hash of the asset path** (stable when other files are
      added, so other assets are not reconverted), with the build checking there are no collisions among the game's
      assets and writing the **data index** (FR-13) that maps hashes to paths and types. Alternatives rejected: a
      path string inside every reference (up to 255 bytes each), a dense index number (adding one file would
      reconvert every asset that references anything). Hashing paths is the same idea the ROADMAP proposes for tags.
- [x] **Are references followed automatically?** — **answer: no** (FR-8). Loading stays explicit and its cost visible
      (constitution V); cycles are free; the game decides what to keep in memory. A helper that loads a whole graph
      can come later if games ask.
- [x] **Missing fields: error or default?** — **answer: the struct's default member initializer**, with `required` in
      the schema for fields that must be set (FR-7). Unknown fields are always errors (they are usually typos).
- [x] **Versioning?** — **answer: no user-visible version field.** Data files are checked against the current struct
      on every build, so they never go stale; each converted asset carries the binary format version and the schema
      fingerprint, and a mismatch (an old converted folder on PC, a stale romfs) is a load error that says to rebuild
      (FR-10/FR-11). Versioned data the *player* creates belongs to the later "Save data with versions" item.
- [x] **Limits** (FR-14; the plan checks them against the Old 3DS budget) — **answer:** text file ≤ 64 KB; line ≤ 1024
      bytes; strings ≤ 255 bytes; lists ≤ 256 items; nesting depth ≤ 4; ≤ 64 fields per type; struct ≤ 16 KB; ≤ 64
      data types and ≤ 4096 data assets per game; ≤ 20 errors printed per file.
- [x] **Where do references resolve to a path, and what does it cost?** — **answer:** in the data index, loaded on the
      first data call (or an explicit preload in a loading scene), tag `data`, about 8 bytes plus the path length per
      entry (a 200-asset game: a few KB), released at shutdown or explicitly (FR-13).
- [x] **Example: new `examples/data` or extend `examples/platformer`?** — **answer: a new `examples/data`** (AC-1). It
      shows every kind and reference in one place, and leaving the platformer untouched demonstrates constitution XI
      (a game that doesn't use data assets). Moving the platformer's tuning into a data file can follow as a small
      change.
- [x] **Hot reload?** — **answer: out of scope** (non-goals); the ROADMAP's "Hot reload of assets in the simulator"
      will reload data assets too, and FR-11's "load into the game's struct" keeps that possible (reload = load
      again into the same struct).
- [x] **Module boundary** — **answer: its own optional module `<nnh/data.hpp>`**, depending only on the foundation
      (assets, `Result`, memory) and on the header-only value types `Vec2` and `Color`; it never loads textures or
      sounds itself, so it doesn't pull in graphics or audio code (XI).
