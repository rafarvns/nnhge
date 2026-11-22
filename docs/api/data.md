# data — typed game data in text files

Header: `<nnh/data.hpp>` · Spec: [014](../../specs/014-data-assets/spec.md) · Plan:
[014](../../specs/014-data-assets/plan.md) · Decision: [D29](../architecture/decisions.md)

> Status: in progress (spec 014 stages 0–5 of 9). Done: declaring types, the loading and listing API (tested on
> the fake platform), the data index format, and `nnh-assetc data` (the TOML-subset reader and the converter). Not
> yet: the `data-index` step, `nnh_add_game(... DATA_SCHEMA ...)` in the build, and `examples/data` — so a game
> can't use data assets end to end yet. This page grows into the full reference with stage 9 (T9.2).

A game describes its enemies, items or a level's settings as plain C++ structs, writes one small text file per
enemy or item in its assets folder, and loads a file into its struct by path. The build checks every file against
the struct and converts it; the console never parses text. Optional (constitution XI): a game that doesn't use it
links no data code.

## Declaring a type

```cpp
struct Item {
    nnh::data::String<24> name;
    int price = 10;
    nnh::data::TextureRef icon;
};
// In a schema source (listed in DATA_SCHEMA, stage 7), at namespace scope:
NNH_DATA_TYPE(Item, "item", NNH_DATA_FIELD(name), NNH_DATA_FIELD(price, .min = 0), NNH_DATA_FIELD(icon));
```

Field kinds: `bool`; 8/16/32-bit integers; `float`; `data::String<N>` (N ≤ 255 bytes); enums declared with
`NNH_DATA_ENUM`; `nnh::Vec2`; `nnh::Color`; nested structs (`NNH_DATA_STRUCT`); fixed arrays `T[N]` and
`data::List<T, N>` (N ≤ 256); references `data::Ref<T>`, `TextureRef`, `SoundRef`, `MusicRef`. Per field:
`.min`, `.max` (numbers) and `.required`. Mistakes are compile errors.

## The text format

A strict subset of [TOML 1.0](https://toml.io/en/v1.0.0); every accepted file is valid TOML. One file per asset,
named `<name>.<key>` (`items/potion.item`):

```toml
# a potion
name = "Potion"
price = 25
icon = "art/potion"          # a reference: a path without extension, "" for none

[stats]                      # a nested struct
hp = 30

[[drops]]                    # one block per item of a list (or array) of structs
item = "items/potion"
chance = 0.25
```

| Value | Form |
|-------|------|
| bool | `true` / `false` |
| integer | `-12`, `1_000` (decimal; must fit the member and `min`/`max`) |
| float | `1.5`, `-0.25`, `1e3`, or an integer a float holds exactly |
| string | `"text"` with `\" \\ \n \t \uXXXX` |
| enum | `"Fire"` |
| `Vec2` | `[x, y]` |
| `Color` | `"#RRGGBB"` or `"#RRGGBBAA"` |
| arrays, lists | `[1, 2, 3]` (may span lines, comments and a trailing comma allowed); of structs: `[[field]]` blocks |

Refused, each with its own message: dates and times, literal (`'...'`) and multi-line strings, inline tables,
dotted and quoted keys, hexadecimal/octal/binary numbers, `inf`/`nan`, a key or a section set twice. Limits: a file
≤ 64 KiB, lines ≤ 1024 bytes, headers of at most 4 parts, arrays nested at most 2 deep.

The build reports every problem of a file as `<path>:<line>: <reason>`, sorted by line, at most 20, with a
suggestion for misspelt names: `assets/enemies/goblin.enemy:7: unknown field 'damgae' (did you mean 'damage'?)`.

## Loading and listing

| Call | |
|------|-|
| `data::load(path, out)` | `out` = `T{}` plus the file's values (`"enemies/goblin"`, no extension) |
| `data::load(ref, out)` | follows a `Ref<T>` the same way |
| `data::count<T>()`, `name<T>(i)`, `ref<T>(i)` | the assets of type T, in path order |
| `data::find<T>(path)` | a `Ref<T>` to that asset if it is a T, else empty |
| `data::path(ref)` | the path a reference names (`load_texture(data::path(item.icon))`) |
| `data::preload()`, `release()`, `memory_used()` | the data index, on purpose |

Errors leave `out` unchanged and are logged: `NotFound` (no such data asset, an empty or unknown reference, a game
without data assets, a type whose schema source isn't in the build), `InvalidArgument` (bad path, an asset of
another type, a type without `NNH_DATA_TYPE`), `Corrupted` (a damaged file, or one converted for another version
of the type: "rebuild the assets"), `Unsupported` (before `nnh::init()`, or a newer format), `OutOfMemory`,
`IoError`. References are never followed automatically.

## Memory

The first data call (or `preload()`, in a loading scene) loads the game's **data index** `.nnh_data` into one
engine block under the tag `data`, together with a scratch buffer the size of the largest data asset:
`32 + 28·types + 10·entries + Σ(path + 1) + largest` bytes (`memory_used()`). After that, `load`, `count`, `name`,
`ref`, `find` and `path` allocate nothing (tested). `release()` frees it; `nnh::shutdown()` frees it too in Debug
and Development builds, before the leak report. A Release build keeps it until the program ends (it has no leak
report, and its assets can't change while it runs), so games that don't use data assets stay byte for byte the
same.

Loaded structs are the game's own (on its stack, in its scene): copy, store and compare them like any struct.
