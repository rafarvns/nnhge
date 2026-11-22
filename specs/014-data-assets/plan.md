# Plan 014: Data assets

Status: implemented (console check pending; Linux fuzz/probe pending) — approved 2026-11-22 (maintainer pre-approved); see the "Stage N refinements"
Spec: [spec.md](spec.md) · Tasks: [tasks.md](tasks.md) · Decision D29 ([decisions.md](../../docs/architecture/decisions.md))
Uses spec 002 (`Result`, memory tags, logs), spec 007 (assets, per-file build steps, the asset index), spec 012 /
D27 (`nnh-assetc`, host tools in cross presets, shared pure validators), spec 013 (console test scripts)

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ✅ | One portable module (`<nnh/data.hpp>`); no platform contract change; the same converter, validator and loader on every system |
| II 3DS constraint | ✅ | Loaded structs are game memory; the engine keeps only the index (+ one scratch the size of the largest data asset), sized below; every limit of FR-14 is checked at build and load |
| III Build speed | ⚠️→✅ | The pressure point: the schema sources are compiled a second time for the host in cross presets. Answered by sharing the objects on PC, rebuilding the host program only when the cross-compiled schema objects change (no `BUILD_ALWAYS`), per-file data steps without C++ and `restat` on the schema description. Measured before/after (T0.1, T7.8, T8.3) |
| IV Event-driven | ✅ | No events: loading, listing and references are queries/commands of the module |
| V No hidden costs | ✅ | No RTTI (kinds deduced by templates, types identified by the address of an inline variable), no exceptions, no `virtual`; loads allocate nothing once the index is loaded |
| VI Spec + test first | ✅ | Declaration, registry, fingerprint, validator, apply, index and loader are pure or fake-platform logic with unit tests; the converter has its own tests and fixture ctests |
| VII Small files | ✅ | ≈ 45 new files, each one concept, all planned ≤ 250 lines; the macro machinery in its own detail header so `data.hpp` stays readable |
| VIII Documented API | ✅ | New `docs/api/data.md`, sections in `assets.md` and `cmake.md`, example `examples/data` |
| IX Clean and legal | ✅ | Own TOML-subset reader (no library), original art and sounds made by a script |
| X Safe by default | ✅ | Three parsers of external input (TOML text in the converter; converted files and the index on the console), all bounded and fuzzed; the converter validates its output with the console's validator |
| XI Opt-in, never in the way | ✅ | No `DATA_SCHEMA` → no host program, no data step, no index, no data code linked (static library objects are only pulled by `NNH_DATA_*` registrations) |

## Design

### Overview

```
           C++ (one source of truth)                     build time (host)                         run time (PC and 3DS)
src/data_types.cpp ── NNH_DATA_TYPE(Enemy, "enemy", …) ──┬─> compiled into the game ───────────────> registry of TypeInfo
   (listed in DATA_SCHEMA)                                └─> <game>-data-schema (host program) ─┐   (static nodes, no RTTI)
                                                              writes nnh_data/schema.nnhschema │            │
assets/enemies/goblin.enemy (TOML subset) ─> nnh-assetc data ◄──────────────────────────────────┤            │
                                             │ checks fields, kinds, ranges, enums            │            │
                                             ├─> assets/enemies/goblin.enemy (binary, NNHD)   │            ▼
                                             └─> nnh_data/refs/enemies/goblin.enemy.refs ─┐   │   data::load("enemies/goblin", e)
every .refs + the asset list ─────────────────> nnh-assetc data-index ◄──────────────────┴───┘     index → file → validate
                                                │ references exist and have the right type,          (fingerprint) → T{} → apply
                                                │ unique names, no hash collisions                    data::count<Item>(), name(i)
                                                └─> assets/.nnh_data (hash → path, type)  ───────>  data::path(enemy.sprite)
```

1. The game declares a plain aggregate struct and, in a **schema source** named in `nnh_add_game(... DATA_SCHEMA
   src/data_types.cpp)`, its table with `NNH_DATA_TYPE` / `NNH_DATA_STRUCT` / `NNH_DATA_ENUM` / `NNH_DATA_FIELD`. The
   macros build constant tables (`TypeInfo`, `FieldInfo`, `EnumInfo`) and one static registration node per table.
2. The same source is compiled into a tiny **host program** with an engine-provided `main` that checks the registry
   (duplicate keys, unknown nested types, depth) and writes a **schema description** (versioned text). It is not linked
   against the engine (FR-3).
3. **`nnh-assetc data`** (generic: no game code in it, D27) reads the description and one data file, checks it,
   writes the **converted file** (field by field, layout-independent, with the type's **fingerprint**) under the
   same asset name, and a small **references sidecar** (each reference with its line).
4. **`nnh-assetc data-index`** reads every sidecar and the game's asset list, checks references, names and hash
   collisions, and writes the **data index** `.nnh_data`.
5. At run time `data::load<T>` finds the asset in the index, reads it into a scratch buffer, validates it against
   the game's own compiled `TypeInfo` (fingerprint included), constructs `T{}` in place and applies the values.

### Decision D29 — where the schema comes from and how the build reads it

The spec chose option (a): the schema is C++ and the build reads it through a host program. The concrete choices:

- **Registration without RTTI**: each type is identified by `detail::tag_of<T>()`, the address of
  `template <class T> inline constexpr char type_tag = 0;` — one definition per program for every `T`, the same
  address in every translation unit. `NNH_DATA_TYPE` defines a constant `TypeInfo` and a static `Registrar` whose
  constructor links it into a list whose head is `constinit` (no static-order problem). `load<T>` searches the list
  by tag (≤ 128 nodes, load time only). Nothing has to be declared in the game's headers.
- **Kinds by templates**: `NNH_DATA_FIELD(hp, .min = 1)` expands, inside the `NNH_DATA_TYPE` that names the struct,
  to `detail::make_field<Self, decltype(Self::hp)>("hp", offsetof(Self, hp), FieldOptions{.min = 1})`. A wrong member
  name fails to compile; an unsupported member type hits a `static_assert` that names the supported kinds; the struct
  must be an aggregate, standard-layout, trivially copyable, ≤ 16 KiB, ≤ 64 fields (`static_assert`s).
- **Enums**: `NNH_DATA_ENUM(Element, Normal, Fire, Ice)` keeps `#__VA_ARGS__` (the names) and the values from a
  constant lambda `[] { using enum Element; return Values{Normal, Fire, Ice}; }()` (C++20 `using enum`): no
  per-name macro expansion, no limit from a FOR_EACH macro; a name that isn't an enumerator fails to compile. Up to
  255 names.
- **The host program** (`<game>-data-schema`): the schema sources + `tools/data_schema/schema_main.cpp` + the pure
  registry files. Not linked against the engine; a schema source that calls engine or game functions fails to link
  there, with the linker naming the function (documented rule: schema sources only declare types and tables).
  - **PC presets**: the schema sources form an OBJECT library linked into both the game and the host program, so
    they are **compiled once**; the program links the small static library `nnh_data_schema_support`.
  - **Cross presets (3DS)**: the same OBJECT library is compiled for the console and linked into the game; the host
    program is an `ExternalProject` (`tools/data_schema/` as a standalone project, the host compiler `NNH_HOST_CXX`
    found like `nnh-assetc`'s), **without `BUILD_ALWAYS`**: its build step depends on the console objects of the
    schema sources (`$<TARGET_OBJECTS:…>`), which Ninja already rebuilds exactly when a schema source or a header it
    includes changes. So an ordinary build of a data game pays nothing for the host program; a schema edit pays one
    host compile and link. (Fallback if the dependency on objects can't be expressed: `BUILD_ALWAYS` like
    `nnh-assetc`, ~0.7 s per build on Windows — decided by the spike T0.2 with numbers: **not needed**, see "Stage 0
    refinements" below.)
  - The program rewrites the description only when its content changes; CMake's Ninja custom commands have
    `restat = 1`, so editing a schema source without changing the schema (a comment, a default value) reconverts
    nothing.
- **The description** (text, versioned, one line per type, enum or field) is what `nnh-assetc` reads; it carries
  kinds, capacities, enum names, `min`/`max`/`required`, nested and referenced types by name, and each type's
  source file for messages. Never offsets or sizes: the converted format does not depend on any compiler's layout.
- **Fingerprint**: CRC-32 (`core/crc32`) over a canonical binary encoding of the type's closure: key, field names in
  order, kinds, scalar widths, capacities, counts, effective `min`/`max` (clamped to the member type's range),
  `required`, enum names in order, nested types' encodings, referenced types' keys. One pure function computes it,
  from the compiled tables at run time and from the parsed description in the converter, so they agree by
  construction (tested by a round trip). It includes `min`/`max`/`required` (more than the spec's list), so a stale
  converted file can never hold a value the current struct forbids.
- **Rejected**: a separate schema file with generated C++ (two places, generated code); parsing C++ headers (fragile);
  embedding the description in a section of the game's ELF/EXE and reading it back (needs ELF and PE readers, data
  conversion would wait for the game's link, and linker scripts may drop the section); loading TOML at run time.

#### Stage 0 refinements (2026-11-22, implemented)

- **The fallback is not needed: the host program's build step depends on the console objects** (spike T0.2, numbers
  in [tasks.md](tasks.md)). The dependency goes through a stamp: `add_custom_command(OUTPUT <stamp> COMMAND cmake -E
  touch <stamp> DEPENDS $<TARGET_OBJECTS:<game>_data_schema_objs>)` and `ExternalProject_Add_StepDependencies(<ep>
  build <stamp>)`; no `BUILD_ALWAYS`. A no-op 3DS build of the spike: 0.31 s, against 1.27 s with `BUILD_ALWAYS`.
- **The step also depends on the host program's own engine sources** (`tools/data_schema/schema_main.cpp` and the
  pure core files it compiles), which are not in the console objects; otherwise an engine update would not rebuild
  the program.
- GCC 13 and clang 18 (VM 120) were not probed by the agent; the maintainer runs the probe there, and T9.1 repeats
  it.

### What is a data file, when the keys are only known at build time

CMake can't know the keys at configure time (they are in C++). So in a game with `DATA_SCHEMA`, every asset whose
extension the asset pipeline doesn't own (not `.png`, `.wav`, `.ogg`, `.tmj`, `.tsj`, `.tiled-*`) goes through
`nnh-assetc data`, which **converts it if its extension is a declared key and copies it otherwise**. The output keeps
the same asset name (`enemies/goblin.enemy` → binary `enemies/goblin.enemy`), so the asset index and the name rules
of spec 007 are unchanged; the text is never packed. Keys are `[a-z][a-z0-9_]{0,15}`; the host program refuses the
reserved extensions above, `nnhmap` and `nnh_data`. A game without `DATA_SCHEMA` keeps today's plain copy step.

### Public API (sketch)

```cpp
// <nnh/data.hpp> — includes <cstddef>, <cstdint>, <type_traits>, <nnh/math.hpp> (Vec2), <nnh/result.hpp> and
// <nnh/detail/data_schema.hpp> (the tables behind the macros). Color is forward-declared (no <nnh/gfx.hpp>).
namespace nnh {
struct Color;
namespace data {

inline constexpr int kMaxStringBytes = 255, kMaxListItems = 256, kMaxFields = 64, kMaxDepth = 4;
inline constexpr std::size_t kMaxStructBytes = 16 * 1024;

/// UTF-8 text of up to N bytes (1 <= N <= 255), always NUL-terminated. Trivially copyable.
template <int N> class String {
public:
    const char* c_str() const;  int size() const;  bool empty() const;
    bool assign(const char* text);                       // false (and truncated at a code point) if too long
    friend bool operator==(const String&, const String&);  friend bool operator==(const String&, const char*);
private:
    char text_[N + 1] = {};
};

/// 0..N values of T (1 <= N <= 256), the count kept; unused slots keep T's default.
template <class T, int N> class List {
public:
    int size() const;  bool empty() const;  static constexpr int capacity() { return N; }
    T& operator[](int i);  const T& operator[](int i) const;   // NNH_ASSERT(i < size())
    T* begin();  T* end();  const T* begin() const;  const T* end() const;
    bool push_back(const T& value);  void clear();
private:
    std::uint16_t count_ = 0;
    T items_[N] = {};
};

/// A reference to another data asset of type T: a 32-bit hash of its path (0 = none). Never followed by itself.
template <class T> struct Ref {
    std::uint32_t id = 0;
    bool empty() const { return id == 0; }
    friend bool operator==(Ref, Ref) = default;
};
struct TextureRef { std::uint32_t id = 0; bool empty() const; friend bool operator==(TextureRef, TextureRef) = default; };
struct SoundRef   { /* same */ };   // load_sound(data::path(r))
struct MusicRef   { /* same */ };   // music::play(data::path(r))

/// Per-field rules, written as designated initializers in this order: NNH_DATA_FIELD(hp, .min = 1, .required = true).
struct FieldOptions {
    double min = detail::kNoMin;     // numbers only (a static_assert on other kinds)
    double max = detail::kNoMax;
    bool required = false;           // the file must set it
};

/// Loads the data asset `path` ("enemies/goblin", no extension) into `out`: `out` becomes T{} (the struct's
/// defaults), then the file's values are applied. Load time only; no allocation once the index is loaded.
/// Errors (out unchanged): NotFound (no data asset of that name), InvalidArgument (bad path; the asset is another
/// type; T has no NNH_DATA_TYPE), Corrupted (damaged file; converted for another version of T — "rebuild the
/// assets"), OutOfMemory (the index), Unsupported (before nnh::init()).
template <class T> Result<void> load(const char* path, T& out);
template <class T> Result<void> load(Ref<T> ref, T& out);          // NotFound for an empty reference

/// Data assets of type T, by path (stable order). The index is loaded on the first call.
template <class T> int count();                                     // 0 on error (logged)
template <class T> const char* name(int index);                     // "enemies/goblin"; "" out of range
template <class T> Ref<T> ref(int index);                           // empty out of range
template <class T> Ref<T> find(const char* path);                   // empty if no such asset of type T

/// A reference's path ("" when empty or unknown). Valid until release() / nnh::shutdown().
template <class T> const char* path(Ref<T> ref);
const char* path(TextureRef ref);  const char* path(SoundRef ref);  const char* path(MusicRef ref);

Result<void> preload();      // loads the index now (a loading scene); errors as load()
void release();              // frees the index (reloaded on the next call); also done by nnh::shutdown()
std::size_t memory_used();   // bytes kept under the memory tag "data" (index + scratch), 0 when released
}  // namespace data
}  // namespace nnh

// In one schema source (listed in DATA_SCHEMA), at namespace scope:
NNH_DATA_ENUM(Element, Normal, Fire, Ice);
NNH_DATA_STRUCT(Drop, NNH_DATA_FIELD(item, .required = true), NNH_DATA_FIELD(chance, .min = 0, .max = 1));
NNH_DATA_TYPE(Item, "item", NNH_DATA_FIELD(name), NNH_DATA_FIELD(price, .min = 0), NNH_DATA_FIELD(icon));
NNH_DATA_TYPE(Enemy, "enemy", NNH_DATA_FIELD(name), NNH_DATA_FIELD(hp, .min = 1, .max = 9999, .required = true),
              NNH_DATA_FIELD(speed), NNH_DATA_FIELD(element), NNH_DATA_FIELD(drops), NNH_DATA_FIELD(hurt_sound));
```

- `NNH_DATA_STRUCT` (no key) is a type that only appears nested (a section `[stats]`, or `[[drops]]` blocks); a keyed
  type may be nested too. `Ref<T>` needs a keyed `T`.
- The templates forward to non-template `detail::` functions with `tag_of<T>()`, `sizeof(T)` and a reset function
  `[](void* p) { ::new (p) T{}; }` (in place: no 16 KiB temporary on the stack; `T` is trivially destructible).
- A registered type that is not in the build's index (its source isn't in `DATA_SCHEMA`) is reported once:
  `type 'enemy' is not in the build's schema: list its source in DATA_SCHEMA`.

**Member kinds** (FR-2), deduced by `detail::make_field`:

| C++ member | Kind | File form |
|------------|------|-----------|
| `bool` | bool | `true` / `false` |
| `std::int8_t` … `std::uint32_t` (also `int`, `short`, `char` types of those sizes, except plain `char`) | int, width and sign kept | `-12`, `1_000` (must fit the type and `min`/`max`) |
| `float` | float | `1.5`, `-0.25`, `1e3`, or an integer (exact when \|v\| ≤ 2^24) |
| `data::String<N>` | string | `"text"` (≤ N bytes UTF-8, no NUL) |
| registered `enum`/`enum class` | enum | `"Fire"` |
| `nnh::Vec2` | vec2 | `[x, y]` |
| `nnh::Color` | color | `"#RRGGBB"` / `"#RRGGBBAA"` |
| struct with `NNH_DATA_STRUCT`/`TYPE` | struct | `[field]` section |
| `T[N]` of any kind above | array (exactly N) | `[a, b, c]`; structs: N `[[field]]` blocks |
| `data::List<T, N>` | list (0..N) | `[a, b]`; structs: `[[field]]` blocks |
| `data::Ref<T>`, `TextureRef`, `SoundRef`, `MusicRef` | references | `"items/potion"`, `""` for none |

Not supported (a `static_assert`): pointers, references, `double`, 64-bit integers, plain `char`, `std::string` and
other non-trivially-copyable members, arrays or lists of arrays/lists, unions.

#### Stage 1 refinements (2026-11-22, implemented)

Found while building the declaration side; the code is `engine/core/include/nnh/data.hpp`,
`engine/core/include/nnh/detail/data_{schema,kinds,tables,macros}.hpp` and `engine/core/src/data/`.

- **Type ids**: the existing `detail::type_id<T>()` (`nnh/detail/type_id.hpp`, the event bus's: the address of a
  `static constexpr` member, one per type) is the tag; no second `type_tag` variable.
- **Four detail headers instead of one**, to keep each under ~150 lines: records (`data_schema.hpp`: `FieldKind`,
  `FieldShape`, `FieldInfo`, `TypeInfo`, `EnumInfo`, nodes, registrars), kind deduction (`data_kinds.hpp`), the
  builders `make_field`/`make_type`/`make_enum` (`data_tables.hpp`) and the macros (`data_macros.hpp`).
  `<nnh/data.hpp>` includes them at its end; it does not yet declare `load`/`count`/... (stage 3).
- **Macro expansion**: each `NNH_DATA_TYPE`/`STRUCT`/`ENUM` becomes one struct named `nnh_data_type_<__COUNTER__>`
  in an anonymous namespace (holding `Self`, the field array and the record) plus a node and its registrar. So
  qualified names work (`NNH_DATA_TYPE(game::Enemy, ...)`), nothing is pasted from the type's name, and a trailing
  `;` is consumed by a `static_assert(true, "")`. A type lists at least one field.
- **Mistakes in field options** (`.min` on a string, a range with no value, a non-whole bound on an integer field)
  and **a field listed twice** can't be `static_assert`s (the values are function arguments): the builders call a
  never-defined function such as `data_error_min_or_max_on_a_field_that_is_not_a_number()` during constant
  evaluation, and the compiler's error names it (the compile-fail ctests check the names). Also refused at compile
  time: `const` members, arrays over 256 values, character types (`char`, `wchar_t`, `char8_t`...).
- **`EnumInfo`** carries the enum's source file (for messages) and its values as `std::int64_t`; enums up to 32 bits.
- **`List<T, N>`** exposes `items_offset()` (static, for the tables); `clear()` gives every used slot `T{}` back.
- **Schema check** (`data/schema_check.{hpp,cpp}`, tests in `test_schema_check.cpp`): also refuses two types or two
  enums with the same C++ name (the description refers to types by name), names over 63 characters (type, field, enum
  and enum value names: bounds for the description reader), and the reserved key `t3x` (the 3DS texture outputs)
  besides `png wav ogg tmj tsj nnhmap nnh_data`. Nesting depth is computed by bounded rounds over the registry (no
  recursion), so a cycle — possible only in a damaged description — reads as too deep.
- **Fingerprint** (`data/schema_fingerprint.{hpp,cpp}`): computed with a new streaming `detail::crc32_update`
  (zlib semantics, declared in `core/crc32.hpp`, defined in `core/crc32_update.cpp`). A nested type is encoded as its own fingerprint, memoized per call, so the cost is
  linear in the schema whatever its shape. Enum *values* and enum and struct *widths/sizes* are not covered (the file
  stores a name's index; apply maps it through the game's table). The encoding starts with the text `NNHD-schema`
  (change it if the encoding changes). Pinned in a test: the tests' `Enemy` is `0xFCEF3C32`.
- **Description format** (version 1) is specified in `data/schema_write.hpp`: `enum <Name> <count> <names...>`,
  `type <Name> <key|-> <fields> <fingerprint> <source>`, `field <name> <one|array N|list N> <kind...> [min v] [max v]
  [required]`, enums then types sorted by name. `min`/`max` are written only when narrower than the member type's
  range (floats with 17 significant digits, so they read back exactly); the type's fingerprint is written too, so the
  reader can check it computes the same.
- Tests: `tests/data_assets/` (the folder `tests/data/` holds fixtures); the test framework's cap `kMaxTests` rose from
  1024 to 2048 (this stage passed 1024 tests).

### Events introduced

| Event | Published by | Payload | Typical listeners |
|-------|--------------|---------|-------------------|
| — | — | — | None (constitution IV): loading and listing are queries/commands. |

### The text format (FR-5, FR-6) — what the reader accepts

A strict subset of TOML 1.0; every accepted file is valid TOML.

- UTF-8 (validated; an optional leading BOM is skipped), LF or CRLF (a lone CR is refused), ≤ 64 KiB, lines ≤ 1024
  bytes. `#` comments, blank lines, `key = value` with bare keys `[A-Za-z0-9_-]+`.
- Values: `true`/`false`; decimal integers (sign, `_` only between digits, no leading zeros, 64-bit range before the
  type check); decimal floats per TOML's grammar (parsed once with `std::from_chars` into `float`: correctly
  rounded and the same on every host, FR-16); basic strings with `\" \\ \n \t \uXXXX` (no surrogates, no NUL);
  inline arrays, which may span lines, with comments and a trailing comma, nested at most 2 deep (a list of `Vec2`).
- Tables: `[a]`, `[a.b]`, `[[a]]`, `[[a.b]]` with TOML's meaning (`[a.b]` under an array of tables refers to its
  last element), at most 4 segments (depth, FR-14). A header's path is resolved against the schema (`[a]` needs a
  struct field, `[[a]]` a list or array of structs).
- Refused, each with its own message: dates and times, literal strings (`'…'`), multi-line strings, inline tables
  `{…}`, dotted or quoted keys, hex/octal/binary, `inf`/`nan`, escapes outside the list, a key or table set twice.
- Required fields are checked in every table the file writes (the root always); a nested section not written keeps
  its defaults unless the nested field itself is `required`.

Errors (FR-9): `<path relative to the game folder>:<line>: <reason>`, sorted by line, at most 20 per file (then
`... and N more`); a syntax error stops reading that file. Unknown fields, enum names and referenced data names
suggest the closest name (edit distance ≤ max(1, length/3)): `assets/enemies/goblin.enemy:7: unknown field 'damgae'
(did you mean 'damage'?)`.

#### Stage 4 refinements (2026-11-22, implemented)

The code is `tools/assetc/src/toml.{hpp,cpp}` (the document and its tree), `toml_parse.cpp` (the driver),
`toml_value.cpp` and the internal `toml_cursor.hpp` / `toml_builder.hpp` (no `toml_value.hpp`); tests
`tools/assetc/tests/test_toml{,_tables,_values}.cpp`; fuzz target `toml_target` (`fuzz_assetc_toml`).

- **The text is checked line by line before it is read**: length, a lone CR, control characters (all but tab;
  DEL too — TOML forbids them even in comments), UTF-8 (core's `utf8_valid`), a byte order mark after the start.
- **Floats go straight to `std::from_chars<float>`** (correctly rounded; tests pin halfway cases such as
  `16777217.0` and `1.000000059604644775390625`). A float outside float's finite range (underflow included) and an
  integer outside 64 bits are kept with `out_of_range` set, so the converter reports them with the field's name.
- A bare word gets `text without quotes: strings are written "Fire"`; `inf`, `nan`, dates, hexadecimal numbers and
  literal strings have their own messages; `\u0000` and surrogates are refused.
- The reader stops at the first problem, structural ones (a key or a table set twice, a header through a value)
  included.

#### Stage 5 refinements (2026-11-22, implemented)

The code is `tools/assetc/src/{schema_read,suggest,diagnostics,data_convert,data_writer,data_refs}.*`,
`data_encode.hpp`, `data_tables.cpp`, `data_values.cpp`, `cli_data.cpp`; tests `test_schema_read.cpp`,
`test_suggest.cpp`, `test_diagnostics.cpp`, `test_data_convert{,_tables}.cpp`, `test_data_writer.cpp`,
`test_cli_data.cpp`; fuzz target `data_target` (`fuzz_assetc_data`). nnh-assetc is version 0.2.0.

- **Schema reader**: a type's id is the address of its own record; an enum's values are its indices (the converted
  format stores indices); blank lines are skipped; at most 1 MiB. After reading: references resolved, then
  `check_schema`, then each fingerprint recomputed and compared with the description's (the error names both).
- **`nnh_assetc_tests` compiles the engine tests' schema** (`tests/data_assets/test_types.cpp`) and core's
  `data_apply.cpp`: compiled tables → description → reader → converter → the engine's validator → apply into the
  compiled struct, a code-vs-file round trip ahead of T7.5 (`List` items are read with `begin()[i]` there:
  `operator[]` asserts through the engine, which the tool doesn't link).
- **Suggestions** use the optimal-string-alignment distance (a swap of neighbours is one edit), so `Fier` → `Fire`
  and `damgae` → `damage` are found within max(1, length / 3).
- **Messages** name the field by path — `stats.hp`, `drops[1].chance`, `flags[0]` — and unknown fields by section
  (`unknown field 'hpp' in [stats]`, `in [[drops]]`); a missing required field is reported at its section's header
  line (`[stats]: missing required field 'hp'`; the root's at line 1); lists and arrays of structs count blocks
  (`drops: 5 blocks, at most 4`, `pair: 1 block, exactly 2`).
- An integer is accepted for a float field whenever a float holds it exactly (not only up to 2^24). A reference
  whose path hashes to 0 is refused by the converter already ("rename the asset"), since it writes the hash.
- **CLI**: `--name <asset name>` (required) gives the sidecar the asset's name (the tool can't derive it from the
  input path); `--show` is optional (the input path by default). Sidecar format version 1, in `data_refs.hpp`:
  `asset <key> <size> <name>` and `ref <line> <kind> <target key|-> <field> <path>` (names last: paths may hold
  spaces). Other files are copied with a sidecar holding only the version line. On failure both outputs are
  removed. Usage errors exit with 2.
- **Fuzzing** `data_target`: a data file of a fixed schema with every kind (declared in the fuzz file like a game's,
  read back through its own description); an accepted one must pass the engine's validator and load within its
  rules; after a NUL byte the input carries a schema description of its own for the reader.
- New source files were formatted with the repository's `.clang-format` (clang-format 23), then their include
  blocks put back in the repository's order (own header, standard headers, project headers).

### Converted file format `NNHD` (version 1)

Little-endian, no alignment (read with `memcpy`), ≤ 64 KiB.

| Part | Content |
|------|---------|
| header (20 B) | magic `NNHD`, u16 version = 1, u16 flags = 0, u32 file size, u32 CRC-32 of the body, u32 schema fingerprint |
| body | the root struct value |

A **struct value** is `u8 n` then `n` pairs `(u8 field index, value)`, field indices strictly increasing (unset
fields are absent and keep their defaults). Values by kind: bool `u8` 0/1; ints in their declared width (1, 2 or 4
bytes); float `u32` IEEE bits (finite); string `u8 length` + bytes (≤ capacity, no NUL, valid UTF-8); enum `u8` name
index (the binary depends only on names, which the fingerprint covers); vec2 2 floats; color 4 bytes RGBA; every
reference `u32` hash (0 = none); struct a struct value; array N element values; list `u16 count` (≤ N) + values.

**Validator** (pure, `data_validate.cpp`, shared with the converter): magic, version (newer → `Unsupported`), size,
CRC, fingerprint (mismatch → `Corrupted` "converted for another version of Enemy: rebuild the assets"), then a walk
bounded by the schema (depth ≤ 4 by construction): field indices in range and increasing, every value in bounds
(ints in their type and `min`/`max`, finite floats in range, strings, enum indices, list counts), no trailing bytes.
**Apply** (`data_apply.cpp`) runs only on a validated buffer and writes through the compiled offsets: ints decoded
to the member's width, strings copied and the rest of the buffer zeroed (loaded structs compare bytewise), enums
through the game's value table, list counts into `List::count_`.

#### Stage 2 refinements (2026-11-22, implemented)

The code is `engine/core/src/data/{data_format,byte_reader,data_validate,data_apply}.*`; tests in
`tests/data_assets/test_data_{validate,apply,layout}.cpp` with the test-only writer `data_file.{hpp,cpp}`; fuzzing in
`tests/fuzz/fuzz_data.cpp` (`data_target`, libFuzzer `fuzz_data`).

- **`validate_data(data, size, registry, type, fingerprint)`** takes the expected fingerprint (the loader computes it
  once per type) and returns a `DataCheck {problem, reason}` like the map validator: `Damaged` (→ `Corrupted`),
  `NewerVersion` (a newer format version or any flag → `Unsupported`), `Stale` (the fingerprint: "converted for
  another version of the type: rebuild the assets" → `Corrupted`, the loader adds the type's name), `OverLimit` (over
  64 KiB → `LimitReached`).
- **The validator also checks `required`** in every struct value a file writes (the root, written sections, list and
  array items): the converter guarantees it, the fingerprint covers it, and checking it costs one bit mask. A struct
  the registry can't resolve (a game whose schema check didn't run) is `Damaged` ("the type's schema is incomplete").
- **Every value takes at least one byte and a failed read stops the walk**, so validation is linear in the file size
  whatever the schema (no 256⁴ loops on a truncated file).
- **Apply is memory-safe on any input**, not only validated files: it clamps list counts and string lengths to the
  member's capacity and stops at an unknown field index, enum index or nesting over 4. The fuzz target applies every
  input, accepted or not, into a struct followed by guard bytes.
- **Bounded UTF-8 check**: `detail::utf8_valid(text, size)` is declared in `text/utf8.hpp` and lives in its own
  `text/utf8_valid.cpp`; the validator refuses NUL bytes separately. Likewise `crc32_update` lives in
  `core/crc32_update.cpp`. Both are separate files, and `crc32()` and `next_codepoint()` are untouched, because the PC
  link pulls whole objects from the static library: a first version that shared code with them made Release `hello.exe`
  580 bytes larger (and `hello.3dsx` 44 bytes larger) without linking any data code. With the split, both sizes equal
  T0.1's.

### Data index `.nnh_data` (version 1)

Written by `nnh-assetc data-index`, an asset of the game (name reserved like `.nnh_assets`), loaded on the first data
call or by `preload()`, validated by the same pure validator in the converter and the console.

| Part | Entry | Size |
|------|-------|------|
| header | magic `NNHX`, version, size, CRC-32, counts, largest data asset (scratch size), section offsets | 32 B |
| types | key (16 B), fingerprint, first entry, count | 28 B each (≤ 64) |
| groups | data, texture, sound, music: first entry, count | fixed |
| entries | u32 path hash, u32 path offset; grouped (data by type, then path order; media by path) | 8 B each |
| lookup | u16 entry indices sorted by (group, hash) for binary search | 2 B each |
| strings | NUL-terminated paths without extension | path + 1 |

A path hash is 32-bit FNV-1a of the path without extension (`core/path_hash.{hpp,cpp}`, pure; tags may reuse it); 0
is refused at build time ("rename it"). Hashes are unique within each group (checked by the index step; data names
are unique across types, FR-4). Media entries exist only for media that some data asset references.

**Memory** (tag `data`, one block): `32 + 28·types + 10·entries + Σ(path + 1) + largest data asset`. The example
(8 data assets, 10 referenced media, paths ~14 chars, largest file ~200 B) ≈ **0.8 KB**; a game with 200 data assets
and 100 media (paths ~20 chars) ≈ **9 KB**. Caps: ≤ 4096 data entries, ≤ 4096 media entries, ≤ 256 KiB of path
strings, so the worst index is ≈ 340 KiB (+ ≤ 64 KiB scratch). Loaded structs are the game's (≤ 16 KiB each). The
compiled tables are constant data in the executable (~48 B per field).

**No allocation per load**: the scratch buffer (largest data asset, recorded by the index step) is allocated with
the index, so `load`, `count`, `name`, `ref`, `find` and `path` allocate nothing once the index is loaded (AC-7). This
refines FR-13's "temporary buffer freed before returning": the buffer is kept with the index instead of being
allocated per load, which is cheaper and still bounded. Released by `release()` and by `nnh::shutdown()` through a
new tiny core hook (`detail::add_shutdown_hook`, ≤ 8 function pointers run before the leak report), so games
without data link no data code (XI).

Spec 007's `kMaxAssetIndexBytes` (64 KiB) is smaller than 4096 data assets' names: raised to 256 KiB (allocated at
its real size, so no cost to games).

#### Stage 3 refinements (2026-11-22, implemented)

The code is `engine/core/src/data/{index_format.hpp,index_validate.*,data_index.*,data_load.cpp,data_list.cpp}`,
`engine/core/src/core/shutdown_hooks.*`, `<nnh/detail/name_hash.hpp>` and `<nnh/detail/data_access.hpp>`; tests in
`tests/data_assets/test_{index_validate,index_lookup,data_index,data_load,data_list,data_alloc}.cpp`,
`tests/core/test_name_hash.cpp`, `tests/app/test_shutdown_hooks.cpp`, with the test-only index writer
`tests/data_assets/index_file.cpp` and the fixture `data_fixture.*`.

- **Path hash = D31's `detail::fnv1a32`** (constexpr, in the public detail header `nnh/detail/name_hash.hpp`, with
  a `(text, size)` overload for a path inside a longer name); no `core/path_hash.{hpp,cpp}`. Its test is plan
  016's `tests/core/test_name_hash.cpp` (published vectors, `""`, a `case` label), so plan 016's T0.3 is done.
- **`NNHX` header**: magic, u16 version, u16 flags, u32 size, u32 CRC (of everything after the header), u16 type
  count, four u16 group counts (data, texture, sound, music), u16 0, u32 largest data asset — 32 bytes. The section
  offsets are **derived from the counts**, not stored (fewer fields to trust), so groups are not a section of their
  own. The types table lists **every keyed type of the build's schema, even one without assets**, so a compiled
  type missing from the index means its source isn't in `DATA_SCHEMA` (the warning) rather than "no assets".
- **The validator checks more than the plan listed**: each path is a valid asset path whose FNV-1a hash equals its
  entry's (non-zero) hash — one hash per entry at load —, paths are strictly increasing within each type and media
  group, type keys are well formed (`[a-z][a-z0-9_]*`, NUL-padded) and unique, the types' entries tile the data
  group, the strings end with a NUL, and the lookup is strictly increasing by (group, hash), which also makes it a
  permutation and hashes unique per group. Type lookups are linear (≤ 64), entry lookups binary.
- **One block for the index and the scratch**: the header is read first through `platform::asset_open` /
  `asset_read_at` (`assets::read` can't read a prefix) to size the block; `memory_used()` = the tag's bytes = file
  size + largest = the plan's formula (a test checks the formula).
- **Shutdown hooks run in Debug and Development only** (`if constexpr (build::kDevToolsEnabled)` in
  `nnh::shutdown()`). A plain call made Release `hello.exe` 1,027 bytes and `hello.3dsx` 124 bytes larger, which AC-6
  forbids; Release has no leak report, so the hook's only job there would be freeing memory the process is about to
  give back. Consequence (documented in `data.md`): in Release the index stays until `data::release()` or the end
  of the program; a re-init keeps the same index, which is right because a program's assets can't change. Tests
  that swap the fake assets call `data::release()` (the fixture's `stop()`); the hook tests are compiled for
  Debug/Development only.
- The log channel `data` and the memory tag `data` are defined in `data_index.cpp`, not with the engine's channels in
  `log/engine_channels.cpp` (which every game links). Each compiled keyed type's fingerprint is computed once per
  index load (a fixed table of 64 entries).
- **Errors** settled: an empty reference or an unknown hash → `NotFound`; a T registered but missing from the index
  → `NotFound` ("the type isn't in the build's schema", plus the load-time warning); an asset larger than the index's
  largest → `Corrupted`; the stale message is `data for another version of Enemy: rebuild the assets` (the 63-byte
  error text), the log line names the asset. `path(Ref<T>)` looks in the data group without checking T; `find<T>`
  checks it.
- **Allocation test**: the counting `operator new` is `tests/framework/alloc_counter.{hpp,cpp}` (created here,
  shared with plans 015 and 016; listed as an `nnh_tests` source so one binary owns the replacement). Errors log a
  warning, and the fake platform's in-memory log file grows `std::` containers, so the error path is checked
  against engine memory only.
- `kMaxAssetIndexBytes` is 256 KiB; spec 007's oversized-index test now uses 262 KiB.

### Limits (FR-14), checked at build and at load

| Limit | Value | Build (message) | Load |
|-------|-------|-----------------|------|
| text file | 64 KiB | converter | — |
| line | 1024 bytes | converter | — |
| string | `String<N>`, N ≤ 255 | `static_assert`; converter (`name: 31 bytes, at most 24`) | validator |
| list | `List<T, N>`, N ≤ 256 | `static_assert`; converter | validator |
| depth | 4 nested struct levels | host program; converter (headers) | by schema |
| fields per type | 64 | `static_assert` | validator (field index) |
| struct size | 16 KiB | `static_assert` | — |
| types per game | 64 keyed, 128 in all, 64 enums of ≤ 255 names | host program | registry check (assert) |
| data assets per game | 4096 (+ 4096 referenced media) | index step | index validator |
| converted file | 64 KiB | converter | validator |
| errors printed | 20 per file | converter / index step | — |

### Build integration

```cmake
nnh_add_game(data
    SOURCES main.cpp scenes/bestiary_scene.cpp
    DATA_SCHEMA data_types.cpp        # compiled into the game and into the host program (FR-3)
    ASSETS assets)
```

- `cmake/nnh_data.cmake` (new): `nnh_add_data_schema(<game> <sources>)` creates `<game>_data_schema_objs` (OBJECT,
  linked into the game; also listed in `SOURCES` is fine, it is deduplicated) and the host program, then the custom
  command `<game build>/nnh_data/schema.nnhschema`. Include directories of the host compile: the engine's public
  headers, the game's source folder and each schema source's folder (documented).
- `cmake/nnh_host_tools.cmake`: host-compiler detection becomes `_nnh_host_cxx()` shared by `nnh-assetc` and the
  schema programs; same messages when none is found (cross presets only).
- `cmake/nnh_assets.cmake`, when the game has a schema: every asset not owned by another step →
  `nnh-assetc data <in> <out> --schema <schema> --refs <build>/nnh_data/refs/<name>.refs --show <path shown>`
  (DEPENDS: the file, the schema, the tool; no depfile needed). Then one step `nnh-assetc data-index --schema <schema>
  --assets-list <nnh_assets_<game>.list> --refs-list <list of .refs> --out <assets>/.nnh_data --show <assets dir
  shown>`; `.nnh_data` joins the asset names, so the asset index lists it. `nnh_asset_index.cmake` reserves the name.
- `nnh-assetc` stays generic and prebuildable (`-DNNH_ASSETC`); its version becomes 0.2.0 (new commands).
- Editing a data file reruns that file's step, the index step, the asset index and (3DS) the `.3dsx` repack — no C++
  compile or link (AC-2). Editing a schema source recompiles that file (console + host on 3DS, once on PC), relinks
  the game and the host program, reruns the description; only if the schema text changed are the data files
  reconverted.

### Files

| File | New/changed | Responsibility |
|------|-------------|----------------|
| `engine/core/include/nnh/data.hpp` | new | public API: value types, refs, `FieldOptions`, load/list/path |
| `engine/core/include/nnh/detail/data_schema.hpp` | new | `TypeInfo`/`FieldInfo`/`EnumInfo`, `tag_of`, `make_field` kind deduction, `Registrar`, the four macros |
| `engine/core/src/core/path_hash.hpp/.cpp` | new | FNV-1a 32 of a path (pure; shared with `nnh-assetc`) |
| `engine/core/src/core/shutdown_hooks.hpp/.cpp`, `app/app.cpp` | new/changed | optional modules' shutdown hooks, run before the leak report |
| `engine/core/src/data/data_format.hpp` | new | `NNHD`/`NNHX` layouts, kind codes, limits (pure; shared) |
| `engine/core/src/data/schema_registry.hpp/.cpp` | new | registration list, lookups by tag and key (pure; also in the host program) |
| `engine/core/src/data/schema_check.cpp` | new | duplicates, unresolved nested/enum/ref types, refs to keyless types, depth, reserved keys, counts (pure) |
| `engine/core/src/data/enum_names.hpp/.cpp` | new | split `"A, B, C"`, name ↔ index (pure) |
| `engine/core/src/data/schema_fingerprint.cpp` | new | canonical encoding → CRC-32 (pure; shared) |
| `engine/core/src/data/schema_write.hpp/.cpp` | new | registry → description text (pure; host program and tests) |
| `engine/core/src/data/data_validate.hpp/.cpp` | new | bounded validator of `NNHD` against a schema (pure; shared; fuzz target) |
| `engine/core/src/data/data_apply.hpp/.cpp` | new | validated bytes → struct memory through offsets (pure) |
| `engine/core/src/data/index_validate.hpp/.cpp` | new | bounded validator + lookups of `NNHX` (pure; shared; fuzz target) |
| `engine/core/src/data/data_index.hpp/.cpp` | new | load/release index + scratch (tag `data`), shutdown hook, missing-type warning |
| `engine/core/src/data/data_load.cpp` | new | `detail::load` by path or hash: entry → read → validate → reset → apply |
| `engine/core/src/data/data_list.cpp` | new | `count`, `name`, `ref`, `find`, `path`, `preload`, `release`, `memory_used` |
| `engine/core/src/assets/assets_internal.hpp` | changed | `kMaxAssetIndexBytes` 256 KiB |
| `engine/core/CMakeLists.txt` | changed | explicit source list (data module, support library `nnh_data_schema_support` for PC presets) |
| `tools/data_schema/CMakeLists.txt`, `schema_main.cpp` | new | the host program: standalone project for cross presets; `main` checks and writes the description (only if changed) |
| `tools/assetc/src/toml.hpp/.cpp`, `toml_parse.cpp`, `toml_value.hpp/.cpp` | new | bounded TOML-subset reader: document (headers, key/values, lines), values, strings, numbers |
| `tools/assetc/src/schema_read.hpp/.cpp` | new | description → registry nodes (bounded) |
| `tools/assetc/src/data_convert.hpp/.cpp`, `data_tables.cpp`, `data_values.cpp` | new | document × schema → values: header paths, fields, kinds, ranges, required, references collected with lines |
| `tools/assetc/src/suggest.hpp/.cpp`, `diagnostics.hpp/.cpp` | new | "did you mean"; `file:line: reason` list (sorted, capped) |
| `tools/assetc/src/data_writer.hpp/.cpp`, `data_refs.hpp/.cpp` | new | `NNHD` writer (then `data_validate`); references sidecar |
| `tools/assetc/src/data_index_build.hpp/.cpp`, `index_writer.cpp` | new | references, names, collisions → `NNHX` (then `index_validate`) |
| `tools/assetc/src/cli.cpp`, `tools/assetc/CMakeLists.txt` | changed | `data`, `data-index` commands; version 0.2.0; pure core files added |
| `cmake/nnh_data.cmake` | new | `nnh_add_data_schema()`: OBJECT library, host program (target or ExternalProject), description step |
| `cmake/nnh_game.cmake`, `nnh_assets.cmake`, `nnh_asset_index.cmake`, `nnh_host_tools.cmake`, `CMakeLists.txt` | changed | `DATA_SCHEMA`; data and index steps; `.nnh_data` reserved; shared host-compiler detection |
| `tests/data_assets/test_*.cpp`, `test_types.hpp/.cpp`, `data_file.hpp/.cpp` | new | unit tests of the module (folder name: `tests/data/` already holds fixtures), the tests' schema, a test-only `NNHD`/`NNHX` writer |
| `tests/data_assets/compile_fail/*.cpp` | new | schemas that must not compile (ctest) |
| `tests/data/data_assets/` | new | fixtures: good files, one bad file per rule, a test game for the build-failure ctest |
| `tests/fuzz/fuzz_data.cpp`, `libfuzzer_data.cpp`, `libfuzzer_data_index.cpp`, `fuzz_targets.*`, `test_fuzz_smoke.cpp` | new/changed | `data_target`, `data_index_target` |
| `tools/assetc/tests/test_toml*.cpp`, `test_schema_read.cpp`, `test_data_convert*.cpp`, `test_data_index.cpp`, `fuzz_assetc.*`, `libfuzzer_assetc_{toml,data}.cpp` | new/changed | converter tests and fuzzing |
| `examples/data/` | new | the example (below) |
| `docs/api/data.md` | new | reference |
| `docs/api/assets.md`, `docs/tech/cmake.md`, `docs/architecture/{decisions,project-structure,security,README}.md`, `docs/REFERENCES.md`, `docs/ROADMAP.md`, `CLAUDE.md`, `README.md` | changed | docs the change makes false |

### Platform work

- pc, 3ds: none (assets are read through the existing contract). The 3DS work is build-side: the host program in the
  cross preset (ExternalProject) and the console check of stage 8.
- fake (tests): none new; tests use the fake platform's assets as tilemap tests do, or feed bytes to the pure
  `detail::` functions.

### Example `examples/data`

A bestiary/shop (AC-1), original art and sounds generated by `examples/data/art/make_art.py` (standard library only,
committed outputs, like the platformer's script).

- Types (`data_types.hpp/.cpp`): `Element` enum; `Stats` (struct: hp `std::uint16_t`, attack, defense);
  `Drop` (struct: `Ref<Item> item`, `float chance` 0..1); `Enemy` (`"enemy"`: name, `Stats stats` section, speed,
  element, `TextureRef sprite`, `SoundRef hurt_sound`, `List<Drop, 4> drops`, `Color tint`, `Vec2 spawn`, `bool
  flags[3]`); `Item` (`"item"`: name, price, `TextureRef icon`, heal, `Ref<Enemy> summons`); `Settings`
  (`"settings"`: title, two colours, `MusicRef music`, `Ref<Enemy> first`).
- Assets: 4 enemies, 3 items (one item `summons` the enemy that drops it: a reference cycle, which is fine),
  `game.settings`, sprites, icons, two WAV sounds, one WAV music loop.
- Top screen: the selected enemy's sprite (tinted) and stats; bottom: every enemy from `count/name`, its drops by
  following `Ref<Item>`; UP/DOWN select, A plays the hurt sound.
- **Self-check** (`check.cpp`, SELECT, or `DATA_DEMO=check`): preloads, loads every data asset of every type,
  follows every reference, checks the errors (`load` of a missing name → `NotFound`, an enemy loaded as an item →
  `InvalidArgument`, `out` unchanged), checks `mem::tag_stats` of `data` unchanged across 100 loads, and computes a
  CRC-32 over every loaded value **written field by field** (never raw struct bytes: layouts differ between PC and
  3DS). Logs `data check: PASS checksum 0x… (4 enemies, 3 items, 1 settings, index N bytes)` or `data check: FAIL
  <what>` and shows a large PASS/FAIL banner on both screens.
- **Console check without reading** (spec 013): `examples/data/tests/check.nnhs` — `fail-on "] E "`,
  `fail-on "data check: FAIL"`, screenshots of the list, `tap SELECT`, `expect "data check: PASS checksum
  0x<value from the PC run>"`. The same script runs first in the simulator (`nnh run data --pc --script …`), so a
  checksum that changed with the data fails on PC before the console. The maintainer only opens the Homebrew
  Launcher and presses Y; `nnh` prints `nnh: PASS`/`FAIL`.

#### Stages 6–9 refinements (2026-11-22, implemented)

**Stage 6 — the index step** (`tools/assetc/src/{data_index_build,index_writer,cli_data_index}.*`, the sidecar reader
in `data_refs.cpp`; tests `test_data_index.cpp`, `test_data_refs.cpp`, `test_cli_data_index.cpp`; fixtures and
ctests in `tests/data/data_assets/`):

- CLI `data-index --out --schema --assets-list --refs-list [--show]` (its own file `cli_data_index.cpp`, like
  `cli_data.cpp`). The sidecar list names files **relative to the list's folder** (or absolute): in cross presets
  devkitPro's MSYS CMake writes `/e/...` paths the native tool can't open (found in the first `3ds-release` build).
- Messages: `drop: no item 'items/potoin' (no file assets/items/potoin.item; did you mean 'items/potion'?)`,
  `drops[1].item: 'enemies/bat' is an enemy, not an item`, `sprite: 'sfx/hit' is a sound, not a texture`,
  `the data asset name 'potion' is also used by assets/potion.enemy: names must be unique across types` (on the
  later file, line 1). Only the first file (by shown path) with problems is printed (≤ 20). Problems of the whole
  index — counts, string bytes, a **media** path hashing to 0 or two media paths sharing a hash — print as
  `<assets folder>: reason`, before any file's. A sidecar of a type the schema lacks: `rebuild the assets`.
- The sidecar reader is strict and bounded (≤ 1 MiB, lists ≤ 4 MiB) and checks every name and path with the asset
  path rules; the mini-fuzz `refs_target` found that a NUL inside a path passed that check (the C string stopped
  early) — now refused. `refs_target` (libFuzzer `fuzz_assetc_refs`) reads mutated sidecars and indexes accepted
  ones against `data_target`'s schema; an accepted index must pass the engine's validator.
- **Every output is rewritten only when its bytes change** — the converted file, its sidecar, the index, and the
  schema description — so Ninja's `restat` stops the chain: a comment edit in a data file reruns that step only; a
  value edit doesn't rerun the index step (the sidecar is unchanged). On failure both tools remove their outputs, so
  the step reruns next time.
- Fixtures: each bad file starts with `# expect: <message>` lines (a TOML comment), which `check_convert.cmake`
  compares with the converter's output exactly; the cross-file cases are folders under `index/` with
  `index/<case>.expect`, run through the real `data` + `data-index` commands by `check_index.cmake`. So one ctest per
  fixture (`data_convert_rejects_<rule>`, `data_index_rejects_<case>`) checks the failure, the line and the exact
  text; there are no separate `explains_` tests. Over-limit files (64 KiB, 1024-byte lines) are generated at
  configure time. The committed `schema.nnhschema` is the tests' schema with sources relative to the repository; the
  ctest `data_schema_program_writes_the_fixtures_schema` keeps it equal to what the tests' schema program writes.
- The TOML reader's size message became `the file is larger than 64 KiB` (the command line reads at most 64 KiB + 1
  bytes, so the old "65537 bytes" was wrong for bigger files).

**Stage 7 — build integration** (`cmake/nnh_data.cmake`, `tools/data_schema/`, `cmake/nnh_assets.cmake`,
`nnh_asset_index.cmake`, `nnh_host_tools.cmake`, `nnh_game.cmake`):

- Names follow CMake targets: `<game>_data_schema` (the program), `<game>_data_schema_objs`, `<game>_data_description`
  (the target producing `nnh_data/<game>.nnhschema`). `nnh_data_schema_support` is defined by
  `tools/data_schema/CMakeLists.txt` (PC: as a subdirectory; 3DS: the same file builds one game's program), not by
  `engine/core/CMakeLists.txt`, so the program's files stay together.
- The schema objects take `nnh::core`'s usage requirements and reach the game as `$<TARGET_OBJECTS>` (not by linking
  the OBJECT library, which would drag a private `nnh::engine` link into test programs that use the fake platform).
- `nnh_use_data_schema(<target> <description> <producer>)` lets a target reuse another's description: the tests'
  "games" `tests/data/data_assets/game` (T7.5, target `nnh_test_data`) and `broken_game` (T7.6) use `nnh_tests`'
  schema, whose source `test_types.cpp` moved out of `nnh_tests`' list into `nnh_add_data_schema(nnh_tests ...)`. The
  3DS side of T7.3 is checked with `examples/data` (tests aren't built in cross presets).
- Without a host compiler, a 3DS configure still succeeds; the data game's schema step fails at build time with what
  to set. `nnh_assetc_command()` takes what needs it (`"data assets"`) for its configure message.
- `.nnh_data` is reserved in `nnh_asset_index.cmake` unless passed in `-DGENERATED=.nnh_data`; a game with a schema
  refuses a user file of that name at configure time.
- D31 applied (plan 016's T7.6): `.ase.json`, `.ase`, `.aseprite` are copied, never routed to the data step; the keys
  `nnhsprite`, `ase`, `aseprite` are reserved in `schema_check.cpp`.
- On Windows CMake writes `.nnh_assets` with CRLF line ends (the engine's parser accepts both); the round-trip test
  strips them. A fixture with CRLF line ends is exempted from the repository's LF normalisation (`.gitattributes`).
- The two ctests that run `cmake --build` on the test game share a `RESOURCE_LOCK` (one Ninja per build folder).

**Stage 8 — the example**: types as planned, with `Element { Normal, Fire, Ice, Earth }`, enemies goblin, imp, slime,
yeti, items potion, ether and the ice crystal (summons the yeti, which drops it: the reference cycle), `game.settings`.
The self-check's allocation check compares `mem::region_stats(Heap)` and `data::memory_used()` across 100 loads
(the `data` tag isn't public); its line is also a PC ctest (`data_self_check_pc`). Index + scratch: 595 bytes.

**Stage 9**: no guide (the plan asks for none); `docs/api/data.md` is the reference. Hand mutation testing of the
new logic: 68 of 72 valid mutants killed, the 4 left equivalent (tasks.md T9.5). The compile-fail ctests'
regexes no longer match a quote character (GCC prints ‘ ’ in a UTF-8 locale on Linux). The Linux runs of T9.1 (GCC 13
and clang 18 probe, ASan/UBSan, the five libFuzzer targets) are left to the maintainer.

## Test strategy

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | `tests/data_assets/test_schema_declare.cpp` (fields, names, offsets, kinds deduced, defaults untouched, options), `test_schema_registry.cpp` (registration, lookups, duplicate key/type, unresolved nested type, ref to keyless type, depth > 4, reserved key — on local registries); ctest `data_schema_rejects_*`/`data_schema_explains_*` (compile-fail: unknown member, pointer, `double`, `std::string`-like member, too many fields, struct over 16 KiB, `min` on a string, unknown enum name); ctest host program with two types of the same key → message naming both sources |
| FR-2 | `test_string_list.cpp`, `test_refs.cpp` (value types, trivially copyable, `==`, empty); `test_schema_declare.cpp` (every kind, every int width, arrays, lists, nested); `test_data_apply.cpp` (each kind applied) |
| FR-3 | `test_schema_write.cpp` + `tools/assetc/tests/test_schema_read.cpp` (description round trip: same fingerprint as the compiled tables); `test_round_trip.cpp` (the tests' schema compiled into `nnh_tests` and into its host program, fixtures converted at build time, loaded back); build ctest of stage 7 |
| FR-4 | `test_data_index.cpp` (converter): duplicate name across types, extension selects the type, non-key files copied unchanged (CLI test) |
| FR-5, FR-6 | `tools/assetc/tests/test_toml.cpp`, `test_toml_values.cpp`, `test_toml_tables.cpp` (every value form, CRLF, BOM, comments, multi-line arrays, trailing commas, nested tables, arrays of tables, each refused feature with its message and line) |
| FR-7 | `test_data_convert.cpp` (defaults kept, required, unknown field + suggestion, wrong kind, int/float ranges and `min`/`max`, string and list lengths, array counts, enum + suggestion) |
| FR-8 | `test_refs.cpp`, `test_data_list.cpp` (path of a reference, following one, cycles); `test_data_index.cpp` (missing data asset + suggestion, wrong type, texture to a sound, sound to an image, `""` allowed, hash collision forced through a test hook) |
| FR-9 | `test_diagnostics.cpp` (line order, 20 cap, relative path); ctest `data_convert_rejects_*`/`explains_*` (one fixture per rule, AC-3); ctest `data_build_fails_with_file_line` (a test game's `_assets` target fails with the exact message); stage 7 `ninja -d explain` checks |
| FR-10 | `test_data_validate.cpp` (header, version, CRC, fingerprint, every bound), `test_data_layout.cpp` (two structs with different member order and padding, same schema → same converted bytes and same values), converter self-validation test |
| FR-11 | `test_data_load.cpp` (fake assets: ok, defaults, not found, bad path, wrong type, stale fingerprint message, damaged, before init, `out` unchanged on every error, by reference, empty reference) |
| FR-12 | `test_data_list.cpp` (count/name/ref/find by type, path order, out of range, a type with no assets) |
| FR-13 | `test_data_alloc.cpp` (memory formula = `memory_used()` = tag stats; no allocation over 100 load/count/list/path calls after `preload`; `release` and shutdown free everything; leak report clean) |
| FR-14 | each limit plus one at build (converter tests) and at load (validator tests) (AC-5) |
| FR-15 | AC-6 checks (T7.8): `hello` builds with the same Ninja steps and the same executable/`.3dsx` size; `platformer` untouched; `nm` of `hello` has no `nnh::data` symbol |
| FR-16 | floats parsed by `from_chars` (tests with hard decimal cases); the converter tests and fixtures ctests run on Windows and Linux (T9.1); the example's checksum equal on PC and 3DS (T8.4) |
| X | fuzz: `data_target`, `data_index_target` (mini-fuzz in `nnh_tests`, libFuzzer `fuzz_data`, `fuzz_data_index`), `fuzz_assetc_toml`, `fuzz_assetc_data` (whole conversion from memory against a fixed schema + the index step); ASan/UBSan on VM 120 |

## Risks and alternatives

- **Host compile in cross presets (III)**: a schema edit costs a host compile + link (~1–2 s on Windows, to measure);
  the first 3DS build of a data game detects the host compiler once (~9 s, like `nnh-assetc`). The spike T0.2 proves
  the "rebuild only when the console objects change" dependency; fallback `BUILD_ALWAYS` (~0.7 s per build, still
  within the 5 s `.3dsx` budget measured at 2.7 s) with the decision written back here.
- **Schema sources that don't compile or link on the host** (they include platform headers, or call functions):
  documented rule, the error names the file; the example and the tests keep schema sources declaration-only. A
  future option, if games need it: `-DNNH_DATA_SCHEMA_FILE=<description from a PC build>`.
- **No host compiler at all** (some CI images): 3DS games with data can't build; others are unaffected (as for maps).
- **Static registration** relies on the schema objects being linked: they are linked directly (OBJECT library), never
  through a static archive, so the linker can't drop them.
- **`using enum` / `offsetof` on templates** across GCC (WinLibs, devkitARM, Linux) and clang: probed in T0.2
  before any code depends on it.
- **Hash collisions** of 32-bit path hashes: refused by the build with both paths (probability ~0.2 % for 4096 names
  in one group); the fix is a rename. A dense index was rejected (adding a file would reconvert everything).
- **Ninja runs data steps in parallel**: two broken files may both print their errors before the build stops; each
  block is still one file's, sorted.
- **Asset index size**: raising `kMaxAssetIndexBytes` to 256 KiB changes spec 007's limit (no memory cost).
- **Rejected alternatives**: run-time TOML on the console (bigger code, float parsing per system, larger fuzz
  surface); one combined converter step for all data files (any edit would reconvert everything); a CMake list of
  keys in `nnh_add_game` (a second source of truth); keeping a temporary buffer per load (heap churn).

## Constitution Check (after design)

| Article | OK? | Note |
|---------|-----|------|
| I | ✅ | Portable module, no contract change; the same pure validator in converter and console; checksum compared PC vs 3DS |
| II | ✅ | Index + scratch ≈ 1 KB in the example, ≤ ~400 KiB worst case (capped); loaded structs ≤ 16 KiB in game memory |
| III | ✅ (measured) | Objects shared on PC; host program rebuilt only on schema changes; data edits compile nothing; numbers in T0.1/T7.8/T8.3 and the ROADMAP |
| IV | ✅ | No events |
| V | ✅ | No RTTI, exceptions or `virtual`; no allocation per load (tested) |
| VI | ✅ | Every FR mapped to unit, converter or ctest tests; the example by screenshot and a scripted console check |
| VII | ✅ | One concept per file; macro machinery in a detail header |
| VIII | ✅ | `docs/api/data.md`, assets/cmake docs, example |
| IX | ✅ | Own reader, generated art |
| X | ✅ | Four fuzz targets (two on the console side, two in the converter); limits at build and load |
| XI | ✅ | Zero steps, zero code, zero memory without `DATA_SCHEMA`; plain `assets::load` stays |

Changed during design: III needed the object-sharing and the objects-as-dependency design (D29); FR-13's temporary
buffer became a scratch kept with the index (to meet AC-7 literally); the fingerprint covers `min`/`max`/`required`
too; spec 007's asset-index cap rises to 256 KiB. No exception to any article.
