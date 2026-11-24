# data — typed game data in text files

Header: `<nnh/data.hpp>` · Spec: [014](../../specs/014-data-assets/spec.md) · Plan:
[014](../../specs/014-data-assets/plan.md) · Decision: [D29](../architecture/decisions.md) · Example:
[`examples/data`](../../examples/data/main.cpp)

> Status: implemented on PC and 3DS builds; the run on a console (the scripted check) and the Linux fuzz/sanitizer
> runs are pending.

A game describes its enemies, items or a level's settings as plain C++ structs, writes one small text file per
enemy or item in its assets folder, and loads a file into its struct by path. The **build** checks every file
against the struct (unknown field, wrong kind, out of range, a missing or wrong reference) and stops with
`file:line: reason`; then it converts the file to a compact binary. The console never parses text. Changing a value
means editing a text file and rebuilding the assets: no C++ is recompiled. Like Unity's ScriptableObjects or Godot's
Resources, without an editor. Optional (constitution XI): a game without `DATA_SCHEMA` gets no extra build step and
links no data code.

## 1. Declare the types (C++)

A data type is a plain struct (an aggregate: no constructors, no virtual functions, no pointers) with default member
initializers. Its **schema** — the fields data files may set, and the file extension of the type — is written once,
in a **schema source**:

```cpp
// data_types.hpp — included by the game and by the schema source
enum class Element : std::uint8_t { Normal, Fire, Ice };
struct Stats { std::uint16_t hp = 10; std::uint8_t attack = 1; };
struct Item;
struct Drop { nnh::data::Ref<Item> item; float chance = 1.0f; };
struct Enemy {
    nnh::data::String<16> name;
    Stats stats;
    Element element = Element::Normal;
    nnh::data::TextureRef sprite;
    nnh::data::List<Drop, 4> drops;
};
struct Item { nnh::data::String<16> name; std::int32_t price = 10; nnh::data::TextureRef icon; };

// data_types.cpp — the schema source, listed in DATA_SCHEMA; at namespace scope:
NNH_DATA_ENUM(Element, Normal, Fire, Ice);
NNH_DATA_STRUCT(Stats, NNH_DATA_FIELD(hp, .min = 1, .max = 9999, .required = true), NNH_DATA_FIELD(attack));
NNH_DATA_STRUCT(Drop, NNH_DATA_FIELD(item, .required = true), NNH_DATA_FIELD(chance, .min = 0, .max = 1));
NNH_DATA_TYPE(Enemy, "enemy", NNH_DATA_FIELD(name, .required = true), NNH_DATA_FIELD(stats),
              NNH_DATA_FIELD(element), NNH_DATA_FIELD(sprite), NNH_DATA_FIELD(drops));
NNH_DATA_TYPE(Item, "item", NNH_DATA_FIELD(name), NNH_DATA_FIELD(price, .min = 0), NNH_DATA_FIELD(icon));
```

```cmake
nnh_add_game(mygame
    SOURCES main.cpp ...
    DATA_SCHEMA data_types.cpp   # compiled into the game and into the build's schema program
    ASSETS assets)               # assets/enemies/goblin.enemy, assets/items/potion.item, ...
```

| Macro | Declares |
|-------|----------|
| `NNH_DATA_TYPE(T, "key", fields...)` | a data type whose files are `*.key`; it can be loaded, listed and referenced |
| `NNH_DATA_STRUCT(T, fields...)` | a struct that only appears nested (a `[section]` or `[[blocks]]`) |
| `NNH_DATA_ENUM(E, names...)` | an enum written by name in files (up to 255 names) |
| `NNH_DATA_FIELD(member, .min = a, .max = b, .required = true)` | a field files may set; options in that order, all optional |

- A field the schema doesn't list is invisible to files and keeps its default. A file that doesn't set a listed field
  keeps the default too, unless the field is `.required`. `.min`/`.max` are for numbers (clamped to the member's
  type).
- Keys: 1 to 16 characters of `a-z 0-9 _`, starting with a letter, unique in the game. Reserved (other asset steps
  own them): `png wav ogg tmj tsj nnhmap nnh_data t3x nnhsprite ase aseprite`.
- Mistakes are **compile errors** where C++ can see them (an unknown member, an unsupported member type, `.min` on a
  string, a field listed twice, a struct over 16 KiB or 64 fields, an unknown enum name), and **build errors**
  naming the schema source(s) otherwise (two types with one key, a nested struct without its own macro, nesting
  deeper than 4).
- **The schema source rule:** the build compiles the schema sources a second time, for your computer, into a small
  program that describes the schema (it is never linked against the engine). So schema sources and the headers they
  include must be plain C++ plus `<nnh/...>` headers, and contain declarations and these tables only — no calls to
  engine or game functions (the linker would name the function). Keep game code in other files.

### Field kinds

| C++ member | In a data file |
|------------|----------------|
| `bool` | `true` / `false` |
| `std::int8_t` … `std::uint32_t` (and `short`, `int`, `unsigned`, … of those sizes; not plain `char`) | `-12`, `1_000` — must fit the type and `min`/`max` |
| `float` | `1.5`, `-0.25`, `1e3`, or an integer a float holds exactly |
| `nnh::data::String<N>` (N ≤ 255 bytes, UTF-8) | `"text"` with `\" \\ \n \t \uXXXX` |
| enum with `NNH_DATA_ENUM` | `"Fire"` |
| `nnh::Vec2` | `[x, y]` |
| `nnh::Color` | `"#RRGGBB"` or `"#RRGGBBAA"` |
| struct with `NNH_DATA_STRUCT`/`TYPE` | a `[field]` section |
| `T[N]` of any kind above | `[a, b, c]` (exactly N); of structs: N `[[field]]` blocks |
| `nnh::data::List<T, N>` (N ≤ 256) | `[a, b]` (0..N); of structs: `[[field]]` blocks |
| `nnh::data::Ref<T>` (T has `NNH_DATA_TYPE`) | `"items/potion"` — a data asset's path without extension; `""` for none |
| `nnh::data::TextureRef` / `SoundRef` / `MusicRef` | `"art/potion"`, `"sfx/hit"`, `"music/theme"` — what `load_texture` / `load_sound` / `music::play` take |

Not supported (a compile error): pointers, references, `double`, 64-bit integers, plain `char`, `std::string` and
other non-trivially-copyable members, arrays or lists of arrays/lists, unions. Every supported kind is trivially
copyable: copy, store, compare and save loaded structs like any plain struct.

The value types (`data::String`, `data::List`, `data::Ref`, the media references) and their kinds serve
[structured saves](saves.md) too (spec 027), which add `double` and 64-bit integers for save files only; including
`<nnh/data.hpp>` for them links no data-asset code.

## 2. Write the files (text)

One file per asset in the `ASSETS` folder, named `<name>.<key>`: `enemies/goblin.enemy`. The extension selects the
type; the **name** (`enemies/goblin`) must be unique among all data assets, whatever their type. The format is a
strict subset of [TOML 1.0](https://toml.io/en/v1.0.0) — every accepted file is valid TOML:

```toml
# A goblin. Comments run to the end of the line; LF or CRLF.
name = "Goblin"
element = "Fire"
sprite = "art/goblin"        # a reference: assets/art/goblin.png must exist

[stats]                      # the nested struct `stats`
hp = 30

[[drops]]                    # one block per item of a list (or array) of structs
item = "items/potion"        # must be an item
chance = 0.25

[[drops]]
item = "items/ether"
chance = 1e-1
```

Refused, each with its own message: dates and times, literal (`'...'`) and multi-line strings, inline tables
`{...}`, dotted (`stats.hp = 1`) and quoted keys, hexadecimal/octal/binary numbers, `inf`/`nan`, escapes outside the
list above, a key or a section set twice, text without quotes.

**Editor highlighting:** map your extensions to TOML. VS Code (`settings.json`):
`"files.associations": {"*.enemy": "toml", "*.item": "toml", "*.settings": "toml"}`; other editors have the same
setting.

### References

`Ref<T>` names another data asset of type T; `TextureRef`, `SoundRef` and `MusicRef` name a `.png`, or a `.wav`/`.ogg`
asset, by the path a game passes to `load_texture`, `load_sound` and `music::play`. The build checks that every
non-empty reference names an existing asset of the right kind. A reference is 4 bytes (a hash of the path): compare it
with `==`, test it with `.empty()`, turn it back into its path with `data::path(ref)`. References are **never
followed automatically** — loading an enemy doesn't load its drops or its sprite — so cycles (an item that summons
the enemy that drops it) are fine. A `TextureRef` names images of the assets folder, not `IMAGES` embedded in the
executable.

## 3. Build errors

The build checks every data file and stops at the first file with problems, printing each one as
`<path from the game's folder>:<line>: <reason>`, sorted by line, at most 20 (then `... and N more`), with a
suggestion for misspelt names:

```text
assets/enemies/goblin.enemy:7: unknown field 'damgae' (did you mean 'damage'?)
assets/enemies/goblin.enemy:9: element: unknown Element 'Fier' (did you mean 'Fire'?)
assets/enemies/goblin.enemy:14: drops[0].item: no item 'items/potoin' (no file assets/items/potoin.item; did you mean 'items/potion'?)
assets/enemies/goblin.enemy:4: sprite: 'sfx/hit' is a sound, not a texture
assets/items/potion.item:1: the data asset name 'items/potion' is also used by assets/items/potion.enemy: names must be unique across types
```

Every rule has a fixture with its exact message in
[`tests/data/data_assets/bad/`](../../tests/data/data_assets/bad/) (one file per rule, the message in its first
lines) and [`index/`](../../tests/data/data_assets/index/) (the cross-file rules).

## 4. Load and list (C++)

```cpp
Enemy goblin;                                       // in a scene member, in on_enter (load time)
if (auto r = nnh::data::load("enemies/goblin", goblin); !r) NNH_LOG_WARN(game, "%s", r.error().message);
Item drop;
if (!goblin.drops.empty() && nnh::data::load(goblin.drops[0].item, drop)) { /* follow a reference */ }
auto sprite = nnh::load_texture(nnh::data::path(goblin.sprite));   // media: the game loads them
for (int i = 0; i < nnh::data::count<Item>(); ++i) shop.add(nnh::data::name<Item>(i));
```

| Call | |
|------|-|
| `data::load(path, out)` | `out` becomes `T{}` (the struct's defaults) plus the file's values; `"enemies/goblin"`, no extension |
| `data::load(ref, out)` | follows a `Ref<T>` the same way (`NotFound` when it is empty) |
| `data::count<T>()`, `name<T>(i)`, `ref<T>(i)` | the data assets of type T, in path order (no hand-written lists) |
| `data::find<T>(path)` | a `Ref<T>` to that asset if it is a T, else an empty one |
| `data::path(ref)` | the path a reference names ("" when empty or unknown); valid until `release()` |
| `data::preload()`, `release()`, `memory_used()` | the data index, on purpose (a loading scene) |

Errors are returned as a `Result` (and logged), never a crash, and leave `out` unchanged: `NotFound` (no data asset
of that name, an empty or unknown reference, a game without data assets, a type whose schema source isn't in
`DATA_SCHEMA`), `InvalidArgument` (a bad path, an asset of another type, a type without `NNH_DATA_TYPE`),
`Corrupted` (a damaged file, or one converted for another version of the struct: "data for another version of Enemy:
rebuild the assets"), `Unsupported` (before `nnh::init()`, a newer format), `LimitReached`, `OutOfMemory`, `IoError`.
Values loaded are bit for bit the same on PC and 3DS (floats are parsed once, by the build).

## 5. Limits

| Limit | Value | Checked |
|-------|-------|---------|
| text file | 64 KiB, lines ≤ 1024 bytes | build |
| string | `String<N>`, N ≤ 255 bytes | compile time, build, load |
| list | `List<T, N>`, N ≤ 256 | compile time, build, load |
| nesting | 4 struct levels below a data type (`[a.b.c.d]`) | build, load |
| fields per type / struct size | 64 / 16 KiB | compile time |
| types per game | 64 data types, 128 types and structs, 64 enums of ≤ 255 names | build |
| data assets per game | 4096, plus 4096 referenced textures, sounds and music; 256 KiB of paths | build, load |
| converted file | 64 KiB | build, load |
| errors printed | 20 per file | build |

## 6. Memory

The first data call (or `preload()`) loads the game's **data index** `.nnh_data` into one engine block under the
memory tag `data`, together with a scratch buffer the size of the largest data asset:
`32 + 28·types + 10·entries + Σ(path + 1) + largest` bytes, where entries are the data assets plus the media they
reference (`memory_used()`). `examples/data` (3 types, 8 data assets, 10 referenced media) measures **595 bytes**; a
game with 200 data assets and 100 media with 20-character paths needs about 9 KB. After that, `load`, `count`,
`name`, `ref`, `find` and `path` allocate nothing (tested, and checked again by the example's self-check). Load in
`on_enter`, not every frame. `release()` frees the index; `nnh::shutdown()` frees it too in Debug and Development
builds, before the leak report. A Release build keeps it until the program ends (it has no leak report, and its
assets can't change while it runs), so games without data assets stay byte for byte the same.

Loaded structs are the game's own (on its stack, in its scene, in its arrays).

## How the build does it

`DATA_SCHEMA` compiles the schema sources once for the game and links the same objects into a host program (PC);
in cross presets (3DS) a host build of them rebuilds only when they change. That program writes a **schema
description**; `nnh-assetc data` checks and converts each data file against it (one build step per file) and
`nnh-assetc data-index` checks the references across files and writes `.nnh_data`. Editing a data file reruns only
its step and the asset index (and repacks the `.3dsx`): no C++ compile or link. Details:
[cmake.md](../tech/cmake.md#data-assets-data_schema); the binary formats: [assets.md](assets.md#data-assets).

## See also

[assets.md](assets.md) (the assets folder, reading other files with `assets::load`) ·
[examples/data](../../examples/data/main.cpp) (a bestiary and shop with a self-check) ·
[testing on the console](../guides/testing-on-the-console.md) (its check script) ·
[prefabs.md](prefabs.md#values-from-data-assets) (one prefab per data asset: a designer adds an enemy by adding a
file) · [particles.md](particles.md#presets-in-text-files) (effect presets: the game's own preset type in its schema,
turned into a `ParticleDesc`; engine types with constructors such as `FrameTime` aren't data types)
