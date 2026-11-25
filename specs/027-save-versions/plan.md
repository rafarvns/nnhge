# Plan 027: Save data with versions

Status: implemented 2026-11-24; Linux checks passed and console runs 1 and 2 done 2026-11-25 (after the T10.3
optimisation round every bench PASS; budgets set) (approved 2026-11-24 overnight)
Spec: [spec.md](spec.md) · Tasks: [tasks.md](tasks.md) · Decisions D45, D46 ([decisions.md](../../docs/architecture/decisions.md))
Uses spec 002 (`nnh::save`'s `NNHS` container, its write procedure and `read_one`, `nnh::fs` paths, `Result`, logs,
asserts, profiler, `detail::crc32`), spec 014 (`data::String`, `data::List`, `data::Ref`, the media references, the
kind traits of `nnh/detail/data_kinds.hpp`, the enum registry; D29 **read, not changed**), spec 017 (an `NNHB` blob
can live in a save; its bytes unchanged), spec 007 (the example ships golden saves as assets), spec 013 (scripts), the
text module's `detail::utf8_valid`, `detail::fnv1a32`. Built in a worktree from `main` and merged **after** specs 022,
023, 024 and 025 (see "Merge notes").

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ⚠️→✅ | Pressure point: "the same bytes and the same values on PC and 3DS" through compilers whose struct layouts differ (i686 aligns `u64` to 4 inside structs, ARM EABI and x86-64 to 8; padding bytes are unspecified) and an FPU that flushes subnormals on the 3DS. Answered: the codec reads and writes members by `offsetof` (never the struct's bytes), every number moves as its bits (floats and doubles included: the codec never uses the FPU, so flush-to-zero can't change a value), exactness and finiteness are integer tests, and the golden checksum is a hash over the members' values in a fixed order, not over struct bytes (AC-2) |
| II 3DS constraint | ⚠️→✅ | Pressure points: the 3DS main thread's small stack, SD write latency (each small SD append cost ≈ 20 ms on the maintainer's Old 3DS), AC-8's targets predating any measurement. Answered: caller buffers sized by a compile-time constant, no allocation, recursion bounded by the 8 nesting levels with small frames (stack estimate 0.6–0.9 KB, checked with `-fstack-usage` before the console, T7.5); a store is exactly `nnh::save::write`'s five file operations; costs estimated per workload below (all expected PASS, the store median tight) |
| III Build speed | ✅ | One header, `<nnh/saves.hpp>`: `<nnh/data.hpp>` (unchanged: value types and kind traits; including it links nothing) + `<nnh/result.hpp>` + `<initializer_list>`; the declarations are constexpr tables like D29's; a game may keep them in one source and wrap store/load in its own functions, so other files see only the struct. Header cost and a game `.cpp`'s edit time measured (T0.1, T2.4, T8.4) |
| IV Event-driven | ✅ | No events (spec constraint IV): `store`/`load` are calls returning `Result`s; the docs show a game publishing its own `GameSaved` |
| V No hidden costs | ✅ | No exceptions (`Result` with reasons), no RTTI (`detail::type_id`, ADL-found constexpr tables), no `virtual` (migration steps are plain function pointers), no heap anywhere in the module |
| VI Spec + test first | ✅ | Pure codec tested first on the fake platform against hand-written bytes; evolution, migration and damage cases; power cuts at every file operation; golden v1/v2/v3 files; fuzz; negative-compile ctests; mutation pass |
| VII Small files | ✅ | One module (`saves`, 12 sources ≤ 250 lines), one public header + four detail headers; one concept per file |
| VIII Documented API | ✅ | `docs/api/saves.md`; `fs.md`, `data.md`, `security.md` linked; `examples/saves`; D45, D46 |
| IX Clean and legal | ✅ | Own code from published designs (protobuf's field numbers and wire types, FlatBuffers' ids and deprecation, Serde's defaults and version chains, Godot, Unity, Butano, Pokémon Gen III's two blocks); cited in `docs/REFERENCES.md` |
| X Safe by default | ⚠️→✅ | Pressure points: a new parser of external data, and FR-7's backup on a CRC-valid but malformed file. Answered: a schema-free structural validator runs over the whole payload before anything is written (bounded: lengths, counts, ids, kinds, depth ≤ 8, canonical varints, valid UTF-8, linear time), fuzzed as given and with the `NNHS` CRC fixed; the load tries `.bak` when the structure is invalid; AC-3 cuts power after every file operation of a store on the fake platform |
| XI Opt-in, never in the way | ⚠️→✅ | Pressure point: sharing spec 014's value types and kinds must not pull the data pipeline in. Answered: saves include `<nnh/data.hpp>` (templates and declarations only) and link none of `src/data/` except `schema_registry.o` (≈ 0.5 KB, the enum names of `NNH_DATA_ENUM`, only reached by a save with an enum field); the data index, loader and validator are never referenced (map-file probe, T8.3); raw `nnh::save` and `NNHB` are byte-identical (pinned); `hello` unchanged |

## Design

### Overview

```
game code                                   <nnh/saves.hpp>  (namespace nnh::save, beside spec 002's read/write)
  struct Progress {...};                      NNH_SAVE_TYPE / NNH_SAVE_STRUCT / NNH_SAVE_SUMMARY in the struct's namespace
  NNH_SAVE_TYPE(Progress, "progress",           → struct nnh_save_tables_Progress { entries, sorted fields, steps,
    NNH_SAVE_VERSION(3),                            retired ids, max size } + constexpr nnh_save_info(SaveTag<Progress>)
    NNH_SAVE_STEP(1, from_v1), ...,             (found by ADL; nested structs found the same way at compile time)
    NNH_SAVE_FIELD(name, 1), ...,
    NNH_SAVE_RETIRED(3, 4));                  encode(value) ──► [structured header][fields, ascending ids]      (pure)
  save::Buffer<Progress> buffer;              store(path)   ──► encode, then nnh::save::write(path, payload, 1)   (one fs_write)
  save::store("slot1.sav", p, buffer);        load(path)    ──► save_read_one(path) → validate(structure) → apply → steps
  save::load("slot1.sav", p, buffer);                            └ on NotFound/Corrupted/IoError/bad structure: path.bak
                                              OldFields     ──► read-only walk of the file's bytes for steps
                                              dump          ──► text lines (Debug/Development only)
spec 002 (unchanged bytes): NNHS header 16 B + payload; temp file, `.bak`, rename; detail::save_read_one = its read_one
```

### Decision D45: save schemas are constexpr tables declared beside the struct and found by ADL; files are self-describing tagged fields in spec 002's container

**Declarations.** A save type is described by one macro at namespace scope **in the struct's own namespace**, with the
struct named by an unqualified identifier (the rule of `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE`), in a header or in one
source file. The macro expands to a struct `nnh_save_tables_<Type>` whose static constexpr members (implicitly inline,
so a header is fine) hold the entries, the fields sorted by id, the retired ids, the migration steps and the type record,
and to a constexpr function `nnh_save_info(nnh::detail::SaveTag<Type>)` returning that record. The engine's templates
call it unqualified, so argument-dependent lookup finds it in the struct's namespace; a nested struct field finds its
struct's record the same way **at compile time** (a missing declaration is a compile error naming the fix). No static
registration, no registry, no build step. Each entry is one of `NNH_SAVE_FIELD(member, id)`, `NNH_SAVE_RETIRED(ids…)`,
`NNH_SAVE_VERSION(n)` and `NNH_SAVE_STEP(from, function)`; checks run in the constexpr builder and stop the build
through undefined `save_error_<what>()` functions (D29's technique): unknown member, unsupported member type, id 0 or
above 65 535, an id twice, a retired id used or listed twice, a missing nested declaration, more than 128 fields, more
than 8 levels, a root over 64 KiB, an array over 65 535 items, a maximum encoded size over 256 KiB, a bad key, version
0, a step for a version ≥ the current one, two steps for one version, **a gap in the step chain**, a version or step
inside `NNH_SAVE_STRUCT`/`NNH_SAVE_SUMMARY`. Kinds are deduced by spec 014's traits (`data_kind_of`, `DataIsList`,
`DataStringCapacity`, `DataRefTarget`, `data_signed_of`, `data::List::items_offset`) behind a save wrapper that first
takes `double` and 64-bit integers (which spec 014 refuses) and has its own element check; `FieldKind` and every data
file are untouched. Member names (`#member`) and type names are kept only in Debug and Development
(`NNH_FLAVOR_RELEASE` makes them `""`; messages then name ids).

**File format** (all little-endian; the engine supports little-endian targets only, `static_assert`ed):

```
NNHS container (spec 002, unchanged): magic "NNHS", version u16 = 1 (the structured format), flags 0, size, CRC-32
payload = structured header + root fields
  0  u32 magic "NNSV"      4  u8 format version (1)     5  u8 key length (1..16)     6  u16 flags (0)
  8  u32 game save version (≥ 1)    12  u32 key hash (FNV-1a 32 of the key)    16  key bytes (a-z 0-9 _)
  then the root struct's fields, filling the payload
field  = wire:u8  id:varint  value          wire bits 0-4: kind, bits 5-6: shape (0 One, 1 Array, 2 List), bit 7: 0
kinds  = 1 Bool (1 B) · 2 U8 · 3 I8 · 4 U16 · 5 I16 · 6 U32 · 7 I32 · 8 U64 · 9 I64 · 10 F32 · 11 F64 (IEEE bits)
         12 Vec2 (2 × F32) · 13 Color (r g b a) · 14 DataRef · 15 TextureRef · 16 SoundRef · 17 MusicRef (u32 hash)
         18 String (len:u8, UTF-8 bytes, no NUL) · 19 Struct (len:varint, fields with strictly ascending ids)
         (an enum is stored as its underlying integer kind; 0 and 20..31 are invalid)
One       value = the kind's bytes | string | struct
Array/List value = count:varint, then
           fixed kinds: count × width bytes (packed)
           String:      bytes:varint, count × string
           Struct:      template = n:varint (1..128), n × (wire:u8 id:varint) with ascending ids;
                        bytes:varint; count × row, a row = each template field's value in template order (no tags)
varint   = unsigned LEB128, canonical (no needless continuation byte), at most 3 bytes (values < 2²¹)
```

Every declared field is written, defaults included (a file says what the game had, whatever the reader's defaults);
arrays write N items, lists their count, strings their length. Encoding the same values with the same schema gives the
same bytes everywhere. **Overhead per field** (FR-5): a one-value field 2 bytes (3 for ids ≥ 128, 4 for ≥ 16 384); a
struct +1–3 (its length); a string +1 (and only its length is stored, not its capacity); an array or list of numbers
+1–3 (count); of strings or structs +2–6 (count, bytes) and, for structs, a template of 1 + 2 bytes per item field
**once per list** instead of a tag per field per item. The structured header is 16 bytes + the key. The reference save
(AC-2, ≈ 4.2 KB) carries ≈ 200 bytes of framing over ≈ 4.0 KB of values: **≈ 5 %** (target < 25 %); tagging every item
field instead of the template would have cost ≈ 35 %.

**Validation** (`save_validate.cpp`, schema-free, run on the whole payload before anything is written): header (magic,
format version — above 1 is `Unsupported`, key length and characters, hash equals the key's, flags 0, version ≥ 1);
then a bounded recursive walk: every varint canonical and in range, ids 1..65 535 strictly ascending in each struct
and template, wire bytes valid, depth ≤ 8, every length and count checked against the bytes left **before** use, a
struct's fields filling exactly its length, a sequence's `bytes` exactly its elements, strings valid UTF-8 without NUL
(`detail::utf8_valid`), templates of 1..128 fields. Every value costs at least one byte and a template entry at most
four, so re-walking a template per row stays linear in the file size. Failures are `Corrupted` with a reason naming the
path: `slot1.sav: field 9[12].4: string is not UTF-8`, `field 7: list count 900 exceeds the file`.

**Rejected** (D45): raw struct bytes (today; layout-dependent), an `.id` option on `NNH_DATA_FIELD` under `DATA_SCHEMA`
(couples saves to the data pipeline and its host program), a hand-written `serialize(archive&)` (read and write orders
drift, no names for the dump, no compile-time size), field names or hashes in the file (renames break, collisions),
fixed offsets per version (no skipping), CBOR/MessagePack/protobuf proper (a dependency or a larger parser), a static
registry of save types (a hidden global, link order, no compile-time sizes or nested checks), omitting default values
(the file would depend on the reader's defaults), zigzag varints for numbers (arrays of numbers would not be packed
copies), a second summary file per slot (a power cut between two files).

### Decision D46: loads are validate → reset → apply by id with exact conversions → ordered migration steps; nothing changes on error

**Load** (`save_file.cpp`, `save_apply.cpp`): read the file into the caller's buffer through spec 002's `read_one`
(made reachable as `detail::save_read_one`, an insertion); validate; check the key (another key: `InvalidArgument`,
"slot1.sav: a 'settings' save, not 'progress'"), the version (newer than the type's: `Unsupported`, "saved by version
5; this game reads up to 3") and the raw case (a payload without `NNSV`: `Unsupported`, "slot1.sav was written by
nnh::save::write (no structured header): read it with nnh::save::read"). Only then the struct becomes `T{}` and the
file's fields are applied, merging the file's ascending ids with the schema's sorted fields (no quadratic lookup); then
the steps run. A missing main file, a CRC failure, an I/O error or an invalid structure tries `<path>.bak` the same way
(the report says so); a key mismatch, a newer version or a raw file does not (they are not damage). Both unusable:
`Corrupted`, both missing: `NotFound`; the struct is unchanged on every error.

**Automatic evolution** (FR-10): a field absent from the file keeps its default; an unknown id is skipped and counted
(retired ids skipped silently); renames and moves are free (ids); integers convert between widths and signedness when
the value fits, integers convert to `float`/`double` when exactly representable (an integer bit-length test, no FPU),
`float` widens to `double` (on the bits: subnormals stay exact); arrays and lists convert into each other and load every
item that fits; strings load whole when they fit; enums take any integer that fits their underlying type and, when the
enum has `NNH_DATA_ENUM` names, is one of their values; nested structs load by their own ids, recursively. **Values that
don't fit** (FR-11): out of range, another kind without one of those conversions (`double` → `float` only when exact,
no float → integer, no bool ↔ integer, One ↔ sequence, struct ↔ value), a non-finite float (bits test), an enum value
outside its names, a bool byte other than 0/1 → the value keeps its default and counts as **dropped** (per value, so one
bad item of an array is one drop); a string longer than the capacity is cut at a UTF-8 character boundary
(`String::assign`'s rule), a list or array with more items keeps the first ones → **adjusted**; an array with fewer items
keeps the defaults for the rest (not counted). Counting stops at 65 535; the first three paths of each count are kept in
Debug/Development.

**Versions and migrations** (FR-12, FR-13): a type's version is `NNH_SAVE_VERSION(n)` (default 1); `NNH_SAVE_STEP(v,
f)` registers `void f(T& value, const nnh::save::OldFields& old)` as the step from v to v + 1. The chain must be complete
at **compile time** (stricter than FR-12's load-time check, which stays as a Debug/Development assert in the runner and
cannot fire for a type that compiled; Release has no such path). A file of version v < c runs steps v … c − 1 in order
after the automatic load; a file of the current version runs none. Steps are infallible (they may log); what they can't
convert keeps its default. `OldFields` answers `has`, `kind`, `shape`, `count` and `get(path, out)` for a field path of
ids and item indexes (`save::path(4, 2)`, `save::path(9).item(3).field(2)`), reading the file's bytes (retired, unknown
and dropped values included) and converting by the same rules; `get` into a save struct decodes that region into `T{}`
by its own ids. A lookup walks the headers of the levels it passes (lengths make skipping O(1) per field) — load-time
cost, documented. The "keep the v1 struct" pattern is just another save type with the old ids and the same key.

**Summaries** (FR-16): `NNH_SAVE_SUMMARY(Type, key, version, fields…)` declares a partial type: the same key, a subset
of ids, the highest version it reads; it loads by the same code (validation of the whole file included), runs no step
(the report's `version` tells the game which one wrote the file), and can't be encoded or stored (compile error).

**Rejected** (D46): fallible steps (a second copy of the struct to keep "unchanged on error": up to 64 KiB of stack or a
buffer), whole-struct conversions only (every update needs code), refusing a file for one field that doesn't fit,
clamping or silent truncation of numbers, loading newer files while skipping their fields (lost at the next store),
refusing to store over a newer file (an extra SD read per store; the docs tell games to ask the player), a load-time-only
chain check (a missing step found by players), trying the backup for a key mismatch or a newer file (a deliberate file
is not damage).

### Public API (sketch)

```cpp
// <nnh/saves.hpp>: <cstddef>, <cstdint>, <initializer_list>, <type_traits>, <nnh/result.hpp>, <nnh/data.hpp>
// (value types and kind traits; including it links no data code). Optional (XI). Reference: docs/api/saves.md.
namespace nnh::save {

inline constexpr std::size_t kMaxEncodedSize = 256 * 1024; ///< structured header included
inline constexpr int kMaxFields = 128;                     ///< saved fields per struct
inline constexpr int kMaxDepth = 8;                        ///< struct levels, the root included
inline constexpr std::size_t kMaxRootBytes = 64 * 1024;    ///< sizeof a save type
inline constexpr int kMaxArrayItems = 65535;               ///< T[N]; data::List keeps its 256
inline constexpr int kMaxId = 65535;
inline constexpr int kMaxKeyLength = 16;
inline constexpr int kReportPaths = 3;                     ///< paths kept per count (Debug/Development)
inline constexpr std::uint16_t kFormatVersion = 1;

enum class WireKind : std::uint8_t { None, Bool, U8, I8, U16, I16, U32, I32, U64, I64, F32, F64, Vec2, Color,
                                     DataRef, TextureRef, SoundRef, MusicRef, String, Struct };
enum class WireShape : std::uint8_t { One, Array, List };

/// A field inside a save: ids from the root, item indexes for arrays and lists (at most 16 steps).
class FieldPath {
public:
    constexpr FieldPath() = default;
    constexpr FieldPath(std::initializer_list<std::uint16_t> ids);
    constexpr FieldPath item(int index) const;
    constexpr FieldPath field(std::uint16_t id) const;
    constexpr int size() const;
    constexpr bool is_item(int step) const;
    constexpr std::uint32_t at(int step) const;
    /// "9[3].2" into `out`; returns the length.
    int format(char* out, std::size_t capacity) const;
private:
    std::uint16_t steps_[16] = {};   ///< an id, or an item index when its bit in item_mask_ is set
    std::uint16_t item_mask_ = 0;
    std::uint8_t count_ = 0;         ///< 36 bytes in all
};
constexpr FieldPath path(std::uint16_t id);
constexpr FieldPath path(std::uint16_t id, std::uint16_t inner);   // and up to 4 ids

/// What a successful load did (FR-15).
struct Report {
    bool used_backup = false;        ///< the file was unusable; its backup was loaded
    std::uint32_t version = 0;       ///< the save version that wrote the file
    int migrated = 0;                ///< migration steps run
    int unknown = 0;                 ///< values skipped: ids the struct doesn't declare (retired ids not counted)
    int dropped = 0;                 ///< values that didn't fit: the field kept its default
    int adjusted = 0;                ///< strings cut, lists and arrays shortened
    FieldPath unknown_at[kReportPaths], dropped_at[kReportPaths], adjusted_at[kReportPaths]; ///< Debug/Development
};

/// A save file's structured header (FR-16), read without decoding or checking the payload's CRC.
struct FileInfo {
    std::uint32_t key_hash = 0;
    char key[kMaxKeyLength + 1] = {};
    std::uint32_t version = 0;
    std::uint32_t size = 0;          ///< payload bytes: what a load's buffer needs
    bool from_backup = false;        ///< the main file is missing or unreadable; this is its backup's header
};

/// Read-only view of the file's fields for migration steps (FR-13). Valid during the step only.
class OldFields {
public:
    bool has(FieldPath path) const;
    WireKind kind(FieldPath path) const;     ///< None when absent
    WireShape shape(FieldPath path) const;
    int count(FieldPath path) const;         ///< items of an array or list; -1 otherwise
    /// The value converted by the load rules into `out` (numbers, bool, enums, Vec2, Color, references,
    /// data::String<N>, or a struct with its own save declaration, decoded into T{} by its ids). false and `out`
    /// unchanged when absent or when it doesn't fit (FR-21).
    template <class T> bool get(FieldPath path, T& out) const;
private:
    const std::uint8_t* root_ = nullptr;     ///< the root's fields inside the caller's buffer
    std::uint32_t size_ = 0;
};

/// The largest encoded size of T (every string, list and array full), structured header included. Compile-time.
template <class T> constexpr std::size_t max_size();

/// Exactly that many bytes, plus `Headroom` for files written by versions with more data (FR-8).
template <class T, std::size_t Headroom = 0> struct Buffer {
    alignas(4) std::uint8_t bytes[max_size<T>() + Headroom];
};

// Memory (FR-8). T needs NNH_SAVE_TYPE (a compile error otherwise; a summary type can't be encoded).
template <class T> Result<std::size_t> encode(const T& value, void* out, std::size_t capacity);
template <class T> Result<Report> decode(const void* data, std::size_t size, T& value);

// Files (FR-6, FR-7, FR-9, FR-16): atomic and checksummed through nnh::save::write; the backup is its `.bak`.
template <class T> Result<void> store(const char* path, const T& value, void* buffer, std::size_t capacity);
template <class T, std::size_t H> Result<void> store(const char* path, const T& value, Buffer<T, H>& buffer);
template <class T> Result<Report> load(const char* path, T& value, void* buffer, std::size_t capacity);
template <class T, std::size_t H> Result<Report> load(const char* path, T& value, Buffer<T, H>& buffer);
Result<FileInfo> info(const char* path);
bool exists(const char* path);              ///< the file or its backup
Result<void> remove(const char* path);      ///< the file, its backup and a temp file left by a cut store

// Text (FR-17): Debug and Development only; in Release these return Unsupported and contain no dump code.
using LineSink = void (*)(void* context, const char* line);
void log_lines(void* context, const char* line);   ///< a sink logging each line (info, channel "saves")
template <class T> Result<void> dump(const T& value, void* scratch, std::size_t capacity, LineSink sink,
                                     void* context = nullptr);
template <class T> Result<void> dump_bytes(const void* data, std::size_t size, LineSink sink, void* context = nullptr);
Result<void> dump_bytes(const void* data, std::size_t size, LineSink sink, void* context = nullptr); ///< ids only
template <class T> Result<void> dump_file(const char* path, void* buffer, std::size_t capacity, LineSink sink,
                                          void* context = nullptr);
} // namespace nnh::save

// Declarations, at namespace scope in the struct's own namespace (a header, or one source file):
//   void progress_from_v1(Progress& p, const nnh::save::OldFields& old);
//   NNH_SAVE_STRUCT(Item, NNH_SAVE_FIELD(kind, 1), NNH_SAVE_FIELD(count, 2), ...);
//   NNH_SAVE_TYPE(Progress, "progress", NNH_SAVE_VERSION(3),
//                 NNH_SAVE_STEP(1, progress_from_v1), NNH_SAVE_STEP(2, progress_from_v2),
//                 NNH_SAVE_FIELD(name, 1), NNH_SAVE_FIELD(level, 2), NNH_SAVE_FIELD(inventory, 9), ...,
//                 NNH_SAVE_RETIRED(3, 4, 16));
//   NNH_SAVE_SUMMARY(SlotSummary, "progress", 3, NNH_SAVE_FIELD(name, 1), NNH_SAVE_FIELD(level, 2), ...);
```

The typed templates are thin: they fetch `nnh_save_info(SaveTag<T>{})` and call non-template core functions taking the
record (`detail::save_encode`, `save_decode`, `save_store`, `save_load`, `save_dump_*`), so the codec is compiled once
in core. Exact names may still change while building; stage notes record any change.

### Events introduced

None (spec constraint IV). The docs show a game publishing its own `GameSaved` after a successful `store`.

### Semantics fixed by the plan

**Store** (`save_file.cpp`): validate the path (`fs::validate_path`, as `save::write`), encode into the caller's buffer
(`LimitReached` if short: nothing written; `InvalidArgument` if a string member isn't valid UTF-8, with an assert in
Debug/Development — a store never writes a file its own load would refuse), then `nnh::save::write(path, buffer, n,
kFormatVersion)` unchanged: `fs_write(tmp)` with the two chunks (one platform write), `fs_size(path)`, `fs_remove(bak)`,
`fs_rename(path, bak)`, `fs_rename(tmp, path)` — the same five operations as a raw save (FR-6, counted on the fake).
A `bool` member is written as `!= 0`; non-finite floats and enum values outside their names are written as they are and
load as the default (documented). In Debug/Development, if the environment variable `NNH_SAVE_DUMP` was set when the
first store ran (read once with `std::getenv` in `save_dump_env.cpp`; the 3DS has no environment, so never there; a
test hook overrides it), the store also writes `<path>.txt` (the dump, through `fs_write` and `fs_append` in 4 KB
pieces from a 4 KB static buffer; not atomic, not counted by FR-6's rule, which concerns the save).

**Load** as D46. One warning line per load that used the backup, dropped or adjusted: `save slot1.sav: backup used
(field 7: list count 900 exceeds the file); v1 -> v3 (2 steps); dropped 1 (8.2); adjusted 2 (1, 9[3].10)` (paths only
in Debug/Development; counts always). A load with nothing to report logs nothing.

**`info`**: `fs_size` + one `fs_read` of 16 + 32 bytes of the main file (else of the backup, `from_backup = true`); checks
both headers, not the CRC. **`exists`**: `fs_size(path)` or `fs_size(path.bak)`. **`remove`**: removes `path`, `.bak`,
`.tmp` (each that exists); `IoError` when one still exists afterwards; succeeds when none existed.

**Decode from memory**: the same validate → checks → reset → apply → steps on a payload in the caller's memory (no
`NNHS`, no backup). **Dump**: one line per field — `name (id) kind = value`, nested structs indented, lists with their
count, strings quoted with `\"`, `\\` and `\xNN` escapes, unknown fields as `#id kind`, retired as `#id kind (retired)`,
floats as `%a`-free decimal with the bits in hex (`1.5 [3FC00000]`), so two dumps compare by text; a first line `save
'progress' v3 (4170 bytes)`.

**Misuse (FR-21)**: an undeclared type, a summary stored or encoded, a gap in the steps → compile errors; a short buffer
→ `LimitReached`; a bad path → `InvalidArgument`; a view asked for a kind that can't hold the value → `false`; a
migration step that throws is impossible (no exceptions); asserts in Debug/Development for a non-UTF-8 string on encode
and for a step pointer that is null at run time (unreachable for compiled types). Never undefined behaviour
(ASan/UBSan, fuzz).

### Files

| File | New/changed | Responsibility |
|------|-------------|----------------|
| `engine/core/include/nnh/saves.hpp` | new | the public API above (≈ 230 lines with comments) |
| `engine/core/include/nnh/detail/save_schema.hpp` | new | `SaveFieldInfo`, `SaveTypeInfo`, `SaveEntry`, `SaveStep`, `SaveTag<T>`, the `save_error_*` declarations |
| `engine/core/include/nnh/detail/save_kinds.hpp` | new | the save kind wrapper over spec 014's traits (`double`, 64-bit integers, arrays to 65 535), element checks with messages |
| `engine/core/include/nnh/detail/save_tables.hpp` | new | `make_save_field`, `make_save_table` (sort, checks, chain, retired), `make_save_type`, `max_size` computation |
| `engine/core/include/nnh/detail/save_macros.hpp` | new | `NNH_SAVE_TYPE/STRUCT/SUMMARY/FIELD/RETIRED/VERSION/STEP`, the Release name rule |
| `engine/core/src/saves/save_wire.hpp` | new | wire kinds, shapes, varint read/write (inline), structured header layout |
| `engine/core/src/saves/save_header.{hpp,cpp}` | new | build and parse the structured header; key and version checks with messages |
| `engine/core/src/saves/save_validate.{hpp,cpp}` | new | the schema-free bounded walk, reasons with paths |
| `engine/core/src/saves/save_encode.cpp` | new | `detail::save_encode` (every kind, templates for struct sequences, packed copies) |
| `engine/core/src/saves/save_apply.{hpp,cpp}` | new | reset + merge-walk by id, sequences, rows, report counting |
| `engine/core/src/saves/save_convert.{hpp,cpp}` | new | integer/float/double/enum/bool conversions by bits; the exactness tests; enum names through the data registry |
| `engine/core/src/saves/save_migrate.cpp` | new | the step chain runner |
| `engine/core/src/saves/old_fields.cpp` | new | `OldFields` lookups and `get` |
| `engine/core/src/saves/save_decode.cpp` | new | `detail::save_decode` (memory): validate → checks → reset → apply → steps |
| `engine/core/src/saves/save_file.cpp` | new | `store`, `load` (backup rules, report log line), `info`, `exists`, `remove` |
| `engine/core/src/saves/save_report.cpp` | new | path recording, `FieldPath::format`, the warning line, the `saves` log channel |
| `engine/core/src/saves/save_dump.cpp` | new | the text dump (Debug/Development; Release stubs return `Unsupported`) |
| `engine/core/src/saves/save_dump_env.cpp` | new | `NNH_SAVE_DUMP` (Debug/Development), the test override |
| `engine/core/src/fs/save.cpp` | changed (insertion at the end) | `detail::save_read_one(...)` forwarding to the anonymous `read_one` (no line of `write`/`read` changes) |
| `engine/core/src/fs/save_format.hpp` | changed (insertion) | the declaration of `detail::save_read_one`, after `validate_save`'s |
| `engine/core/CMakeLists.txt` | changed | one `_nnh_saves_sources` block (merge notes' anchor) |
| `engine/platforms/fake/include/nnh/platform_fake.hpp`, `src/fs_fake.cpp` | changed (insertions) | `cut_power_after(ops, torn)`, `restore_power()`, `truncate_file(path, size)`: mutating fs calls after the cut do nothing and fail |
| `tests/saves/saves_fixture.hpp`, `test_wire.cpp`, `test_header.cpp`, `test_validate.cpp`, `test_declare.cpp`, `test_encode.cpp`, `test_encode_sequences.cpp`, `test_round_trip.cpp`, `test_evolution.cpp`, `test_fit.cpp`, `test_versions.cpp`, `test_migrations.cpp`, `test_old_fields.cpp`, `test_summary.cpp`, `test_file.cpp`, `test_backup.cpp`, `test_power_cut.cpp`, `test_slots.cpp`, `test_dump.cpp`, `test_blob.cpp`, `test_raw_unchanged.cpp`, `test_save_cost.cpp`, `test_save_misuse.cpp` | new | FR-1–FR-21, AC-1, AC-3, AC-7 |
| `tests/saves/compile_fail/*.cpp` | new | the negative-compile list (AC-1) |
| `tests/saves/test_reference.cpp`, `test_golden.cpp`, `make_saves_golden.cpp` | new | AC-2; the golden writer (an `EXCLUDE_FROM_ALL` executable) |
| `tests/data/saves/v1.sav`, `v2.sav`, `v3.sav`, `v1.txt`, `v2.txt`, `v3.txt`, `README.md` | new | AC-2 golden files and dumps; the README says how and why they were written |
| `tests/fuzz/fuzz_saves.{hpp,cpp}`, `libfuzzer_saves.cpp`, `test_fuzz_saves.cpp` | new | AC-4 (own header: `fuzz_targets.hpp` untouched) |
| `tests/platform_fake/test_power_cut_fake.cpp` | new | the fake's new controls |
| `tests/CMakeLists.txt` | changed | one 027 block + ctests (merge notes' anchors) |
| `examples/saves/` | new | `main.cpp`, `demos.hpp`, `progress.hpp` (v3 types and declarations), `progress_history.hpp` (v1, v2), `progress_migrate.cpp`, `progress_sum.{hpp,cpp}`, `reference.{hpp,cpp}`, `summary.hpp`, `settings.{hpp,cpp}` (declarations in one source), `difficulty_names.cpp` (`NNH_DATA_ENUM`), `tiny_game.{hpp,cpp}`, `slots_panel.{hpp,cpp}`, `hud.{hpp,cpp}`, `showcase_scene.{hpp,cpp}`, `check_scene.{hpp,cpp}`, `bench.{hpp,cpp}`, `bench_scene.{hpp,cpp}`, `assets/golden/v1.sav`…`v3.sav`, `CMakeLists.txt`, `tests/check.nnhs` |
| `examples/CMakeLists.txt` | changed | `add_subdirectory(saves)` |
| `scripts/check_release_strings.sh` | changed (insertion) | the dump's and the report's Debug-only texts |
| docs (stage 11) | new / changed | `docs/api/saves.md` (new); `fs.md`, `data.md`, `architecture/security.md`, `project-structure.md`, `decisions.md` (D45/D46 implemented), `patterns.md`, `docs/REFERENCES.md`, `CLAUDE.md`, `docs/ROADMAP.md` |

### Data and memory

| Item | 3DS | 64-bit PC | Notes |
|------|-----|-----------|-------|
| field record (`.rodata`) | 28 B | 40 B | name, offset, stride, items offset, count, capacity, id, wire kind, shape, flags, nested record, enum id |
| type record | ≈ 40 B + 8 B per step | ≈ 64 B + 16 | fields, retired ids (≤ 64 per struct), steps, max size, key, hash, version |
| the example's `Progress` declarations | ≈ 0.9 KB | ≈ 1.3 KB | 16 root fields, 4 structs |
| codec `.text` (encode, validate, apply, convert, view, file) | ≈ 7–10 KB | ≈ 10–14 KB | estimates checked at T8.3; dump ≈ 3 KB more in Debug/Development only |
| `schema_registry.o` | ≈ 0.5 KB | ≈ 0.8 KB | linked only when a save has an enum field (enum names) |
| heap | 0 | 0 | no allocation anywhere (FR-8) |
| static | 4 KB + 8 B | 4 KB + 8 B | the dump file buffer and the env flag, Debug/Development only |
| caller buffer | `max_size<T>()` (+ headroom) | same | the reference save: ≈ 4.9 KB maximum, ≈ 4.2 KB used |
| `Report` | ≈ 350 B | ≈ 350 B | 9 paths of 36 B + the counts (returned in the `Result`) |
| stack | 0.6–0.9 KB | | validate/apply recurse once per struct level (≤ 8, frames ≈ 64 B) + the top frames; `-fstack-usage` checked (T7.5); target < 1 KiB besides the struct and buffer |

Limits (the spec's, confirmed): ids 1–65 535; ≤ 128 saved fields per struct; ≤ 8 levels; a save type ≤ 64 KiB in memory;
arrays ≤ 65 535 items; lists as spec 014 (≤ 256 items); strings ≤ 255 bytes; an encoded save ≤ 256 KiB (spec 002's
container allows 16 MB); keys of 1–16 characters `a-z 0-9 _` starting with a letter; ≤ 64 retired ids per struct.

### Platform work

- **pc:** none (spec 002's `stdio_fs.cpp`). The dump's environment variable is read by core with `std::getenv`.
- **3ds:** none (same file layer; `std::getenv` returns null).
- **fake (tests):** `cut_power_after(int ops, bool torn = true)`: the next `ops` mutating calls (`fs_write`, `fs_append`,
  `fs_rename`, `fs_remove`) run, the one after is cut (a write keeps half its bytes when `torn`, a rename or remove does
  nothing), and every later mutating call does nothing and returns false until `restore_power()` or `reset()` (reads keep
  working, as after a reboot); `truncate_file(path, size)`. Three insertions in `fs_fake.cpp`'s mutating functions
  (`if (power_is_off()) return false;` with the cut logic in one helper) and the definitions at its end.

### The reference save and the golden files (AC-2)

The example's own state is the reference type (`examples/saves/progress.hpp`), compiled into `nnh_tests`. Version 3:

| id | member (v3) | type (v3) | history |
|----|-------------|-----------|---------|
| 1 | `name` | `String<24>` | v1 `String<12>` (grew in v2) |
| 2 | `level` | `u16` | v1 `u8` (widened in v2) |
| 3 | — | retired | v1–v2 `coins u32`; step 2: `gems = coins / 10` |
| 4 | — | retired | v1 `weapon u16`; step 1: the first inventory item, equipped |
| 5 | `play_frames` | `u64` | v1 `play_time u32` (renamed and widened in v2) |
| 6 | `position` | `Vec2` | v1 |
| 7 | `explored` | `bool[1024]` | v1 (the explored-room flags) |
| 8 | `hero` | `Hero {1 hp, 2 max_hp, 3 attack, 4 defense: u8}` | v1 `stats` without `defense` (added v2, renamed v3) |
| 9 | `inventory` | `List<Item, 64>`; `Item {1 kind u16, 2 count u8, 3 quality u8, 4 durability u16, 5 wear float, 6 bonus u32, 7 dye Color, 8 slot u8, 9 equipped bool, 10 label String<8>}` | v1 `List<Item, 32>` without 7 and 10 (grew in v2; `dye` v2, `label` v3) |
| 10 | `checkpoint` | `data::Ref<Checkpoint>` | v1 |
| 11 | `difficulty` | `enum Difficulty : u8 {Easy, Normal, Hard}` (`NNH_DATA_ENUM`) | v2 |
| 12 | `gems` | `u32` | v3 (step 2) |
| 13 | `room` | `u16` | v3 (step 2: high half of `location`) |
| 14 | `tile` | `u16` | v3 (step 2: low half) |
| 15 | `seed` | `u64` | v1 |
| 16 | — | retired | v2 `location u32` (room << 16 \| tile), split by step 2 |
| 17 | `best_time` | `double` | v2 |
| 18 | `bestiary` | `u16[512]` | v2 |
| 19 | `quests` | `List<Quest, 32>`; `Quest {1 id u16, 2 state QuestState u8 enum, 3 progress u16, 4 note String<16>}` | v1 without `note` (v3) |

Reference values (`reference.cpp`: a seeded FNV-1a sequence, the same function for every version): a 12-byte name with a
two-byte UTF-8 character, 64 items (32 in v1), 1024 flags, 512 bestiary counts, 20 quests, a seed above 2³², a play time
above 2³², a `best_time` with a fractional part and one float `wear` that is subnormal (the flush-to-zero check). The
v3 file is ≈ 4.2 KB (max ≈ 4.9 KB), v1 ≈ 2.5 KB, v2 ≈ 4.0 KB. `progress_history.hpp` declares `ProgressV1`/`ProgressV2`
(the old structs with their ids, key `progress`, versions 1 and 2) — what wrote the goldens, and the docs' "keep the old
struct" pattern. `make_saves_golden` (built by hand, `EXCLUDE_FROM_ALL`) writes `tests/data/saves/v{1,2,3}.sav` (whole
`NNHS` files) and their dumps `v{1,2,3}.txt` (dumped with the v3 schema: retired and unknown ids by number); the
example's `assets/golden/` holds byte-identical copies (ctest `saves_golden_copies_match`). `progress_sum()` is FNV-1a
over every member's value in id order (strings to their length, lists to their count, floats and doubles as bits) —
never over struct bytes (padding, and `u64` alignment differs between i686, x86-64 and ARM). Tests: each golden loads
with the v3 code to its recorded sum and report (v1: migrated 2; v2: 1; v3: 0; unknown, dropped, adjusted 0), encoding
the reference values gives `v3.sav`'s payload byte for byte, each file dumps to its `.txt`. Goldens are regenerated only
with a reason in the commit message (`tests/data/saves/README.md`).

### ARM11 cost plan (AC-8)

Lessons from specs 019–025 on the Old 3DS (ARM11 268 MHz, 16 KB L1D with 32-byte lines, no L2, ≈ 110 cycles per miss,
instruction counts 1.3–2× low): the upper end of each estimate is multiplied by 1.3. Built in: (1) **no FPU in the
codec** — floats are copied as words, exactness and finiteness are integer tests, so no `vmrs`, `vcvt` or
`__aeabi_l2d`-style calls; (2) **packed copies**: a sequence whose file kind, width and sign equal the field's is one
`memcpy` (little-endian everywhere), `bool` arrays one byte test per item; (3) **merge-walks**: ascending file ids against
sorted schema fields, rows walked in lockstep with the template, nothing quadratic and no lookup table on the stack;
(4) **no division** (`__aeabi_uidiv`) in the codec; (5) `pld` on the payload in the validator's sequential walk; (6) the
enum registry is searched once per sequence field, not per item.

| AC-8 workload | Hot path | Estimate (upper × 1.3) | Target | Verdict |
|---------------|----------|------------------------|--------|---------|
| encode the reference save into a buffer (avg of 100) | ≈ 18 root fields; 64 rows × 10 values ≈ 30 instructions each; two packed copies (1 KB each); 20 quest rows; ≈ 130 output lines written | 0.22–0.3 ms (0.39) | < 0.5 ms | expected PASS |
| validate and decode it (avg of 100) | validate ≈ 17 k instructions + 130 lines read ≈ 45 k cycles; reset `T{}` (≈ 4.3 KB of stores ≈ 13 k cycles); apply ≈ 640 item values × ≈ 40 instructions + 1024 bool tests + one 1 KB copy + strings ≈ 75 k cycles | 0.45–0.55 ms (0.72) | < 1 ms | expected PASS |
| decode a v1 file with two steps (avg of 100) | a 2.5 KB file (32 items, no bestiary) ≈ 0.3 ms + steps: 3 `OldFields` lookups over ≈ 15 root headers (≈ 2 k instructions), one inventory insert | 0.3–0.4 ms (0.52) | < 1.2 ms | expected PASS |
| store to the SD card (median of 10; none over 300 ms) | encode 0.3 ms + `fs_write(tmp)` (create, 4.2 KB, close: 20–35 ms) + `fs_size` (3–8) + `fs_remove(bak)` (10–20) + 2 × `fs_rename` (10–20 each) | 55–105 ms (137) | < 150 ms, max ≤ 300 | expected PASS, **tight**; the maximum depends on the card |
| load from the SD card (median of 10) | `fs_size` + 2 × `fs_read` (open, read, close: 3–8 ms each) + decode 0.5 ms | 10–25 ms (33) | < 50 ms | expected PASS |
| load falling back to the backup (median of 10) | the main file read and refused (3 operations + validation) + the backup's 3 operations + decode | 20–50 ms (65) | < 100 ms | expected PASS |
| three slot summaries for "Load game" | 3 × (3 operations + whole-file validation ≈ 0.2 ms + a partial apply ≈ 0.05 ms) | 30–75 ms (98) | < 150 ms in all | expected PASS |
| spec 002's raw `save::write` of the same bytes (logged, no target) | the five operations, no encode | 55–105 ms | — | the structured share = the difference (≈ 0.3 ms) |

T7.5 checks the disassembly and stack use against this table before the console; T10.2 measures; a missed target is
optimised first (fewer file operations within FR-6 — for example asking the platform for `rename` over an existing file
is **not** allowed: it changes spec 002; then the codec), then its budget is set from the measurement with ≈ 25 % margin
and written in the spec, `bench_scene.cpp` and `docs/api/saves.md` with the numbers and the card's name.

### The example `examples/saves` (AC-5, AC-6)

Original shapes and the built-in font only. `SAVES_DEMO=showcase` (default) | `check` | `bench`; `SAVES_SHOT=empty|saved|
golden|backup` for the screenshots (fixed inputs, fixed frame). **Top screen**: a tiny game in a 20 × 12 room grid — a
square moved by the D-Pad or Circle Pad (one room per screen; entering a room sets its `explored` flag), coins (collected
by touching: `gems += 1`), items lying in rooms (picked up into the inventory), the HUD: name, level, gems, play time,
room, inventory count, and the last load report (`backup`, `v1 -> v3`, `steps 2`, `unknown/dropped/adjusted`).
**Bottom screen** (320 × 240): three slot rows (y 8–63, 72–127, 136–191), each with the slot's summary from
`NNH_SAVE_SUMMARY` (name, level, gems, play time) or "empty" and four buttons — Save (x 8–79), Load (88–159), Delete
(168–239), Damage (248–311: rewrites the slot with a valid CRC over a broken structure, to show the backup); a bottom row
(y 200–231): Volume − (8–47), Volume + (56–95), Language (104–175), Load golden v1 (184–255), Rename (264–311: the next name
from a fixed list, one with a two-byte character). Settings (`settings.cpp`: volume, language, a `controls` blob) are a
second save type in `settings.sav`, declared and stored inside that one source (the "declarations in one file" pattern).
Buttons: L/R select the slot, X save, A load, B delete, Y bench, SELECT check, START quit. The simulator's
`NNH_SAVE_DUMP=1` writes `slot1.sav.txt` beside the save. Log lines: `saves: showcase slots <s1> <s2> <s3>` (each `empty`
or `L<level>`), `slot <n>: stored <bytes> bytes`, `slot <n>: loaded sum <sum> version <v> steps <k> unknown <u> dropped <d>
adjusted <a>[ backup]`, `slot <n>: deleted`, `slot <n>: damaged`, `slot <n>: selected`, `settings: volume <v> language
<name> stored`, `golden v1: loaded sum <sum> steps 2`, `name: <name>`.

**Check mode** (`check_scene.cpp`; SELECT or `SAVES_DEMO=check`): removes the three slots and `golden_v*.sav`, logs
`saves: check started`; copies each golden from the assets (`nnh::assets::read`) to `golden_v<n>.sav` in the data folder
(`fs::write_file`; the SD card on the 3DS), loads it and logs `saves: v1 sum <S1> migrated 2`, `saves: v2 sum <S2>
migrated 1`, `saves: v3 sum <S3> migrated 0`; encodes the reference values and logs `saves: encode crc <C> bytes <N>`;
stores the reference into `slot1.sav` twice (values A, then B), reloads and logs `saves: slot sum <SB>`; damages
`slot1.sav` (a valid CRC over a broken structure) and logs `saves: backup used sum <SA>`; reads the three summaries and
logs `saves: summaries L<l> empty empty`; removes slot 1 and logs `saves: removed slot1 exists 0`; then `saves: PASS` or
`saves: FAIL <first mismatch>`. Expected values are recorded from pc-debug in `check_golden.hpp` (`kSavesGolden`).
**Bench** (`bench.{hpp,cpp}` workloads, `bench_scene.{hpp,cpp}` timing; Y or `SAVES_DEMO=bench`): one measured
operation per frame where it touches the SD card, ten per frame for the in-memory ones; the lines `saves bench: encode avg
X ms (target < 0.5 ms) PASS|FAIL`, `decode avg …`, `decode v1 avg …`, `store median X ms max Y ms (target < 150 ms, max
300 ms)`, `load median …`, `backup median …`, `summaries median …`, `raw write median X ms (no target)`, then `saves
bench: PASS` or `saves bench: FAIL (<names>)`; a Debug build logs `not judged (Debug)`.

## Test strategy

All unit tests run in `nnh_tests` on the fake platform (Windows; VM 120: GCC 13/14, clang 18, ASan/UBSan). Expected
bytes are written by hand in the tests (hex arrays with a comment per field), never produced by the encoder under test.

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | `test_declare.cpp`: tables of a type, a struct, a summary (sorted fields, steps, retired ids, version, key, hash, max size), a type in a game namespace, declarations in a header and in one source, a struct that is also an `NNH_DATA_TYPE`; negative-compile ctests (below) |
| FR-2 | `test_evolution.cpp`: renamed and reordered members load by id; a retired id in the file skipped without counting; `test_declare.cpp`: ids per nested struct |
| FR-3 | `test_encode.cpp`: every kind (bool, 8–64-bit signed and unsigned, float, double, enum, `Vec2`, `Color`, `String`, `Ref<T>`, the three media refs, nested struct, `T[N]`, `List`, nested arrays of structs); `test_blob.cpp`: an `ActionSet`'s `NNHB` (`save_to`) in a `List<std::uint8_t, 256>` stored, loaded, `load_from` gives the same bindings |
| FR-4 | `test_encode.cpp` / `test_encode_sequences.cpp`: hand-written expected bytes (little-endian, no padding, ascending ids whatever the declaration order); AC-2's v3 bytes on every compiler and the console (encode CRC line) |
| FR-5 | `test_validate.cpp`: unknown and retired fields of every kind and shape skipped by length; `test_dump.cpp`: a file dumped by ids without its schema; overhead per field asserted in `test_encode.cpp` |
| FR-6 | `test_file.cpp`: the file is `NNHS` (version 1, flags 0) + structured header; one `fs_write` per store and the same fs call sequence as `nnh::save::write` (fake counters); key mismatch message names both keys |
| FR-7 | `test_backup.cpp`: a CRC-valid broken structure, a bit flip, a truncation, a missing main file → backup loaded, `used_backup`; both bad → `Corrupted`, struct unchanged |
| FR-8 | `test_save_cost.cpp`: no allocation (counter) over store, load, encode, decode, steps, view, dump; no `virtual` (none declared); `max_size<T>()` exact (a full value encodes to exactly it) and constexpr (`static_assert`); `Buffer<T>`; memory encode/decode equal the file's payload |
| FR-9 | `test_file.cpp`: `T{}` then fields then steps (a step sees loaded values); the struct unchanged on every error path (a canary pattern compared byte for byte); `NotFound` |
| FR-10 | `test_evolution.cpp`: added, removed, retired, renamed, reordered, widened (u8→u16, i16→i64, u32→u64), signedness changes that fit, int→float/double exact, float→double (a subnormal kept), array/list capacity grown, array↔list, string capacity grown, nested struct evolution |
| FR-11 | `test_fit.cpp`: out of range, sign that doesn't fit, int→float inexact (2²⁴ + 1), double→float inexact, kind changed (int→struct, struct→int, one→array), NaN/∞ (float, double, `Vec2`), enum outside names, bool bytes 2 and 0xFF, string cut at a 2- and 3-byte character boundary, list and array shrunk, array with fewer items; counts and paths |
| FR-12 | `test_versions.cpp`/`test_migrations.cpp`: chains 1→3 and 2→3, order (a log of calls), steps see the struct after the automatic load, a current-version file runs none, the runner's assert (a hand-built record with a null step, `run_expecting_assert`), infallible steps' defaults |
| FR-13 | `test_old_fields.cpp`: `has/kind/shape/count/get` on retired, unknown and dropped values, nested paths `4.2`, items `9[3].2`, a struct region into a save struct, kinds that can't hold the value (false); the old-struct pattern (`ProgressV1` loading `v1.sav`) |
| FR-14 | `test_validate.cpp`: every bound (varints overlong or > 3 bytes, ids 0/65 536/not ascending/duplicate, wire bytes, depth 9, lengths and counts past the end, `bytes` mismatches, templates of 0/129 fields, UTF-8 and NUL), messages with paths; `test_versions.cpp`: newer version → `Unsupported` with the text; raw `nnh::save` file → its message; format version 2 → `Unsupported`; linear time (a 256 KiB adversarial file of nested sequences validated in a bounded count of steps) |
| FR-15 | `test_file.cpp`: report fields; one warning line exactly when backup/dropped/adjusted (log capture), none otherwise; paths only in Debug/Development (pc-release run of the same test checks empty paths) |
| FR-16 | `test_slots.cpp`: `exists` (file, backup only, none), `remove` (file, backup, temp; none existing → ok), `info` (main, backup, raw file), three slots; `test_summary.cpp`: partial loads, a newer file refused, store of a summary is a compile error; settings beside progress |
| FR-17 | `test_dump.cpp`: a value, a buffer, a file; names, ids, nested indentation, list counts, quoted and escaped strings, unknown and retired fields; the env flag writes `<path>.txt` (test override); Release stubs (pc-release); `check_release_strings.sh` (T8.2) |
| FR-18 | AC-2's goldens and sums on WinLibs GCC, GCC 13/14, clang 18 (32-bit host build if available: `-m32` on VM 120), and the console; no FPU instruction in the codec (T7.5) |
| FR-19 | `test_save_cost.cpp`: operation counters (fields visited) linear in size for 128-field structs and long lists; the bench (AC-8) |
| FR-20 | include check (the header pulls `<nnh/data.hpp>`, `<nnh/result.hpp>` and the standard headers named; no gfx, audio, tilemap, entities header); `test_raw_unchanged.cpp`: the bytes of `nnh::save::write` for a fixed blob (CRC pinned at T0.2), the `NNHB` blob of a fixed set (pinned), `link.sav`'s format untouched; spec 002/017 tests unchanged; probes (T8.3): no `data_load`/index symbol in a saves game, `hello` unchanged |
| FR-21 | `test_save_misuse.cpp`: short buffer → `LimitReached` and nothing written, bad paths → `InvalidArgument`, non-UTF-8 string on encode (assert + `InvalidArgument`), view kinds; negative-compile ctests for the compile-time cases |
| AC-1 | all of the above; negative-compile ctests `saves_declaration_rejects_<case>` / `saves_declaration_explains_<case>` for: unknown member, pointer, `std::string`-like member, union, `char`, `const` member, id 0, id 65 536, an id twice, a retired id used, a retired id listed twice, nested struct without a declaration, 129 fields, 9 levels, a 64 KiB + 1 root, an array of 65 536 items, a max size over 256 KiB, a bad key (upper case, 17 characters, empty), version 0, a step for the current version, two steps for one version, a gap in the steps, `NNH_SAVE_VERSION` in a struct, storing a summary, storing an undeclared type (each with GCC's and clang's wording); `saves_declaration_accepts_good` with `-Werror` |
| AC-2 | `tests/saves/test_golden.cpp`, ctest `saves_golden_copies_match` |
| AC-3 | `test_power_cut.cpp`: for a first store and for a store over an existing save, cut after 0, 1, 2, 3, 4 operations (torn and not) and a second store after each cut; the next load gives the new or the previous values, never `Corrupted`, never `NotFound` once a save existed; `test_backup.cpp`'s damage cases |
| AC-4 | `tests/fuzz/test_fuzz_saves.cpp` (mini-fuzz, every compiler), `fuzz_saves` libFuzzer on VM 120 |
| AC-5 | PC screenshots read back (T7.1, T7.2) |
| AC-6 | `examples/saves/tests/check.nnhs` with `--pc` (ctest `saves_check_script_pc`), ctest `saves_self_check_pc`, then the console |
| AC-7 | `test_raw_unchanged.cpp`; spec 002/017 tests and fuzz targets; `data check: PASS checksum 0xD5BD3020`; `actions: PASS 7/7`; `hello` sizes and `nm`; probes; build times (T8.4) |
| AC-8 | the bench lines on the console; T7.5's table |
| AC-9 | stage 11 |

## Risks and alternatives

1. **SD timings** (AC-8): the store median is estimated 55–105 ms (137 with margin) against 150, and the maximum depends
   on the card's housekeeping. Mitigation within FR-6 is limited (the five operations are spec 002's); then AC-8's
   measurement-with-margin policy, with the card named in the docs. Fewer file operations would change spec 002's
   procedure for every save (a follow-up spec, not this one).
2. **ADL declarations** need the macro in the struct's namespace and an unqualified name: a struct nested in a class or
   a template instance can't be declared directly (documented; the error message says so). Alternative kept in reserve:
   `NNH_SAVE_TYPE_AS(tag, Type, …)` taking a separate identifier.
3. **Compile-time chain check is stricter than FR-12** (a gap doesn't compile). Flagged (decision 6); the spec's
   Release no-op path is then unreachable and its assert stays as defence.
4. **The enum names live in spec 014's registry** (`NNH_DATA_ENUM`), so a save with an enum field links
   `schema_registry.o` (≈ 0.5 KB, no loader or index): AC-7's "no data-asset loading code" holds (probe T8.3), "no data
   code at all" would not. Without `NNH_DATA_ENUM` an enum takes any value of its underlying type.
5. **`std::getenv` in core** for `NNH_SAVE_DUMP` (Debug/Development only): no other core file reads the environment
   (the platform does); a contract function was rejected because `system.hpp` is being extended by spec 026 and the
   feature is a simulator convenience. Flagged (decision 15).
6. **Header weight**: `<nnh/saves.hpp>` includes `<nnh/data.hpp>` (≈ 800 lines with its detail headers). Measured at
   T2.4 against `<nnh/data.hpp>` alone (+0.05 s budget); fallback: move spec 014's value types into a detail header — an
   edit of `data.hpp` that would conflict with spec 024's `FieldOptions` hunk, so only if the budget is missed.
7. **Golden files drift**: a codec change that moves a byte breaks AC-2 on purpose; regeneration needs a reason in the
   commit (README), and the example's copies are compared by ctest.
8. **Power cuts on real hardware** are not automated (AC-3 is on the fake). The optional battery-pull test is listed
   for the maintainer (decision 21) and not required.
9. **Struct layout differences** (i686 `u64` alignment 4) never reach the file (offsets come from `offsetof` per
   compiler); the golden sums avoid struct bytes. A 32-bit host run (`-m32` on VM 120, if the multilib is installed) is
   attempted in stage 9; if unavailable it is recorded, the 3DS being the 32-bit check.
10. **Merging after 022–025**: shared files only get insertions at distinct anchors (below); `hello` and every other golden
    are unaffected because no existing code path changes.

## Merge notes (worktree from `main`; merged after specs 022, 023, 024 and 025)

Spec 027 is built in its own worktree (branch `spec-027`) from `main` and merged after the others. Every edit to a
shared file is an insertion at its own anchor; nothing is reordered or reformatted. Rebase on `main` at the start of
stages 5, 7 and 11 and before the merge.

| Shared file | Others | 027's change, at a distinct anchor |
|-------------|--------|------------------------------------|
| `engine/core/CMakeLists.txt` | 022: transform/hierarchy blocks after the prefabs block; 023: a transition block after the camera block and `input_block.cpp` in the main list; 024 and 026: blocks after the hierarchy block's `endif()`; 025: a block after the `_nnh_query_sources` block | one `_nnh_saves_sources` block (no `-ffp-contract` option: the codec has no float arithmetic) **right after the line `target_sources(nnh_core PRIVATE ${_nnh_prefabs_sources})`**, before the `# Transforms (spec 022, D37)` comment; nothing in the main list |
| `tests/CMakeLists.txt` | 023: a block after the `camera_example` line, ctests after `camera_self_check_pc`; 024: a block after the `entities_example` line, ctests after `entities_self_check_pc`; 025: a block before `add_test(NAME unit …)`, ctests after `data_self_check_pc` | **one** block (every 027 test, the example sources the tests compile, `target_sources(nnh_fuzz_targets PRIVATE fuzz/fuzz_saves.cpp)`, the golden writer, the negative-compile ctests in their own `if()` copying the data-schema block's condition, and an `if(NNH_BUILD_FUZZERS)` with `fuzz_saves`) **right after the `queries_example/test_self_check.cpp` line**; ctests `saves_self_check_pc`, `saves_check_script_pc`, `saves_golden_copies_match` **right after the `queries_self_check_pc` block's `endif()`**; nothing in the alphabetical list, `nnh_fuzz_targets`' first line or the libFuzzer `foreach` |
| `examples/CMakeLists.txt` | 023: `transitions` after `text`; 024: `particles` after `prefabs`; 025: `pathfinding` between `input` and `platformer` | `add_subdirectory(saves)` **right after `add_subdirectory(queries)`** |
| spec 014's data module (`data.hpp`, `data_schema.hpp` `FieldKind`/`FieldInfo`, `data_kinds.hpp`, `data_tables.hpp`, `src/data/*`, `tools/assetc`) | 024 amends `FieldOptions`, `FieldInfo`, `data_tables.hpp`, the writer and validator (`.ascending`, `.up_to`) | **no edit**: saves only include `<nnh/data.hpp>` and use the traits listed in D45 (`FieldKind` not even appended: saves have their own `WireKind`); after merging 024, `test_declare.cpp` and the goldens are rerun |
| `engine/core/src/fs/save.cpp`, `save_format.hpp` (spec 002 internals) | nobody | `save.cpp`: a `namespace nnh::detail { … save_read_one … }` block **appended after `} // namespace nnh::save`**; `save_format.hpp`: one declaration **after `validate_save`'s**; `write`/`read`/`read_one` untouched |
| `engine/platforms/fake/include/nnh/platform_fake.hpp`, `src/fs_fake.cpp` | 026 adds 3D recording elsewhere in the fake | declarations **after `corrupt_file`'s**; in `fs_fake.cpp` one guard line at the top of `fs_write`, `fs_append`, `fs_rename`, `fs_remove`, the state next to `g_failing_writes`, the reset in `fs_reset()`, definitions **after `corrupt_file`'s** |
| `scripts/check_release_strings.sh` | 024 may add a line | one `forbidden=` line **after the input-actions line** |
| `tests/framework/test_main.cpp` (`kMaxTests` = 4096) | 023–026 may raise it | raised only if T0.3 finds fewer than 400 left; the larger value wins |
| `CLAUDE.md` | 023: after "Camera helpers" and the `queries` command; 024: after "Sprite animation" and the `nnh run data` line; 025: after "Collision layers and queries" and the `ANIMATION_DEMO=check` line; 026: at the end | the layout paragraph **after the "Input actions (spec 017 …)" paragraph**; the commands **after the `NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/shot.png … hello.exe` line** |
| `docs/ROADMAP.md` | their own items | only the existing "Save data with versions" item |
| `docs/architecture/decisions.md`, `patterns.md`, `security.md`, `docs/REFERENCES.md`, `specs/README.md` | their rows, change-log lines, blocks | D45/D46 rows after D44 and one change-log line (done with this plan); the "Versioned schema" pattern row after "Time-sliced job" (done); security's parser table: one row appended; REFERENCES: 027's block appended; the index row by the lead — on conflict keep both sides |
| `docs/api/fs.md`, `docs/api/data.md` | 024 edits `data.md` (`.ascending`, `.up_to`) | fs.md: a link in the "Saves" section; data.md: one sentence at the end of its value-types section — distinct from 024's options section |

After the merge the lead runs: the full ctest (pc-debug, pc-release), every example's `--pc` script, AC-2's goldens,
022/023/024/025's goldens and the platformer replay ctest with the tree's lines, `data check: PASS checksum 0xD5BD3020`,
and one 3ds-release build. No other golden may move because of 027 (no existing code path changes).

## Stage 10: console run (prepared)

T10.1 on the PC first: `scripts/dkp.sh cmake --build --preset 3ds-release` and `scripts/dkp.sh cmake --build --preset
3ds-dev --target saves_3dsx actions_3dsx data_3dsx`, without a warning; T7.5's table still describes the objects;
3ds-release `hello.elf` equal to T0.1's. On the Old 3DS the SD card keeps files between runs: the check mode removes its
own files first, so the runs are repeatable. Three runs, each: open the Homebrew Launcher, press **Y**, then on the PC
(`nnh` builds the Development `.3dsx`, sends it, plays the script, brings the console back; logs and screenshots in
`build/logs/`). Nobody touches the console after Y.

1. `build/pc-debug/tools/nnh/nnh run saves --script examples/saves/tests/check.nnhs` (≈ 1.5 min). The script (every
   `expect` followed by an input has a `wait 3` after it):
   - `fail-on "] E "`, `fail-on "saves: FAIL"`, `fail-on "saves bench: FAIL"`; `wait 30`; `tap SELECT`;
   - `expect "saves: check started" 30`; `expect "saves: v1 sum <S1> migrated 2" 60`; `expect "saves: v2 sum <S2> migrated
     1" 60`; `expect "saves: v3 sum <S3> migrated 0" 60`; `expect "saves: encode crc <C> bytes <N>" 30`; `expect "saves:
     slot sum <SB>" 60`; `expect "saves: backup used sum <SA>" 60`; `expect "saves: summaries L<l> empty empty" 60`;
     `expect "saves: removed slot1 exists 0" 30`; `expect "saves: PASS" 30`; `wait 5`; `screenshot check`;
   - `tap SELECT`; `expect "saves: showcase slots empty empty empty" 30`; `wait 3`; `screenshot empty`;
   - `tap X`; `expect "slot 1: stored <B1> bytes" 30`; `wait 3`; `hold RIGHT 40`; `tap X`; `expect "slot 1: stored <B2>
     bytes" 30`; `wait 3`; `tap R`; `expect "slot 2: selected" 10`; `wait 3`; `tap X`; `expect "slot 2: stored <B3> bytes" 30`;
     `wait 3`; `touch 290 220` (Rename); `expect "name: <N2>" 10`; `wait 3`; `untouch`; `tap R`; `expect "slot 3:
     selected" 10`; `wait 3`; `tap X`; `expect "slot 3: stored <B4> bytes" 30`; `wait 5`; `screenshot saved`;
   - `touch 220 216` (Load golden v1); `expect "golden v1: loaded sum <S1> steps 2" 30`; `wait 3`; `untouch`; `wait 5`;
     `screenshot golden`;
   - `touch 280 36` (Damage slot 1); `expect "slot 1: damaged" 30`; `wait 3`; `untouch`; `touch 120 36` (Load slot 1);
     `expect "slot 1: loaded sum <SL1> version 3 steps 0 unknown 0 dropped 0 adjusted 0 backup" 30`; `wait 3`; `untouch`;
     `wait 5`; `screenshot backup`;
   - `touch 200 100` (Delete slot 2); `expect "slot 2: deleted" 30`; `wait 3`; `untouch`; `touch 76 216` (Volume +);
     `expect "settings: volume <v> language <name> stored" 30`; `wait 3`; `untouch`;
   - `tap Y`; `expect "saves bench: started" 30`; `expect "saves bench: encode avg" 120`; `expect "saves bench: decode avg"
     120`; `expect "saves bench: decode v1 avg" 120`; `expect "saves bench: store median" 300`; `expect "saves bench: load
     median" 120`; `expect "saves bench: backup median" 200`; `expect "saves bench: summaries median" 200`; `expect "saves
     bench: raw write median" 300`; `expect "saves bench: PASS" 30`; `wait 10`; `screenshot bench`; `end`.

   Expected, with the values recorded in "Stage 6 results" and "Stage 7 results": every logic line equal to the PC's
   (sums, CRC, byte counts — FR-4/FR-18 across PC and 3DS); the bench (from T7.5): encode 0.2–0.4 ms, decode 0.45–0.7,
   decode v1 0.3–0.5, store median 55–140 ms (**tight**; max card-dependent), load 10–35, backup 20–65, summaries
   30–100 — PASS expected, the store the most likely FAIL. A FAIL fails the run (`fail-on "saves bench: FAIL"`) after every
   number is logged: record them in "Console run 1"; T10.3 applies the optimisation order or sets the budgets with margin.
   Read the screenshots (check, empty, saved, golden, backup, bench): the same pictures as the `--pc` run's.
2. `build/pc-debug/tools/nnh/nnh run actions --script examples/actions/tests/check.nnhs` (≈ 40 s). Expected, unchanged
   since spec 017 (the `NNHB` file goes through the untouched `save::write`): `actions: saved 40 bytes sum …` with the
   tree's value, `actions: reloaded 2 applied 0 skipped sum …`, `actions: PASS 7/7`; `nnh: PASS`.
3. `build/pc-debug/tools/nnh/nnh run data --script examples/data/tests/check.nnhs` (≈ 20 s). Expected, unchanged since
   spec 014: `data check: PASS checksum 0xD5BD3020 (4 enemies, 3 items, 1 settings, index 595 bytes)`; `nnh: PASS`.

Optional (not required; decision 21): the maintainer may copy `slot1.sav` from the SD card (`sdmc:/3ds/saves/`) into the
simulator's data folder and load it there (the same summary), and may pull the battery during a loop of stores (a
`SAVES_DEMO=stress` mode is **not** built unless he asks).

## Decisions taken (overnight, to review)

The spec's "Decisions to take" are approved as recommended. Its open questions and these plan-level decisions were taken
overnight, each the recommended or simplest option; the maintainer reviews them in the morning:

1. **Names**: module `saves`, header `<nnh/saves.hpp>`, namespace `nnh::save` (beside spec 002's `read`/`write`):
   `store`, `load`, `encode`, `decode`, `info`, `exists`, `remove`, `dump`, `dump_bytes`, `dump_file`, `log_lines`,
   `max_size`, `Buffer<T, Headroom>`, `Report` (not `LoadReport`, which is spec 017's `nnh::LoadReport`), `FileInfo`,
   `OldFields`, `FieldPath`/`path()`, `WireKind`, `WireShape`; macros `NNH_SAVE_TYPE`, `NNH_SAVE_STRUCT`,
   `NNH_SAVE_SUMMARY`, `NNH_SAVE_FIELD`, `NNH_SAVE_RETIRED`, `NNH_SAVE_VERSION`, `NNH_SAVE_STEP`; log channel `saves`;
   example `examples/saves` (`SAVES_DEMO`, `SAVES_SHOT`), env `NNH_SAVE_DUMP`.
2. **Declarations found by ADL** (`nnh_save_info(SaveTag<T>)` in the struct's namespace, unqualified struct name), constexpr
   tables in a header or one source; no registry, no static initialisers.
3. **Kinds**: spec 014's traits through a save wrapper; `double` and 64-bit integers added **in the save wrapper only**
   (spec 014's `FieldKind` and data files untouched; open question "data types gain 64-bit and double": no).
4. **Wire format** as D45: a 1-byte wire kind + varint id per field, canonical 3-byte-max LEB128 varints, packed number
   sequences, one template per list of structs, `bytes` lengths on variable sequences, every declared field written.
5. **Container**: the payload goes through the unchanged `nnh::save::write` with `NNHS` version 1 (= the structured
   format) and flags 0; the raw case is recognised by the inner magic `NNSV`, not by an `NNHS` flag (no spec 002 change).
6. **The migration chain is checked at compile time** (a gap is a compile error), stricter than FR-12's load-time
   assert; steps are `NNH_SAVE_STEP(from, function)` entries.
7. **Conversions**: only FR-10's list (no float → integer, no bool ↔ integer, `double` → `float` only when exact; array ↔
   list allowed); every number moves as bits; exactness by integer tests.
8. **Drops count per value** (one bad item = one drop); an array with fewer items is not "adjusted".
9. **Key mismatch** → `InvalidArgument`; **newer version**, **raw file**, **newer format** → `Unsupported`; none of them
   tries the backup.
10. **Encoding refuses a non-UTF-8 string** (`InvalidArgument`, assert in Debug/Development); bools written as 0/1;
    non-finite floats and enums outside their names written as they are (they load as defaults; documented).
11. **Summaries** are a third declaration, `NNH_SAVE_SUMMARY(Type, key, version, fields…)`: same key, a subset of ids,
    no steps, refused for newer files, never stored.
12. **Open question "refuse to store over a newer file"**: no; `Unsupported` on load and the docs (ask the player).
13. **Open question "field names in Release"**: stripped (member and type names become `""` under
    `NNH_FLAVOR_RELEASE`; keys stay); messages use ids; the dump is Debug/Development only.
14. **Report**: counts always; up to 3 paths per count in Debug/Development; one warning line per load that used the
    backup, dropped or adjusted.
15. **`NNH_SAVE_DUMP`** read by core with `std::getenv` once (Debug/Development), a test override; the dump file written in
    4 KB pieces (not atomic).
16. **The backup is read through spec 002's own `read_one`**, made reachable as `detail::save_read_one` by an appended
    forwarding function (no line of spec 002 changes).
17. **Fake power cuts**: `cut_power_after(ops, torn)`, `restore_power()`, `truncate_file()` in the fake platform.
18. **The reference save is the example's own `Progress`** (v1/v2/v3 history above), goldens in `tests/data/saves/` with
    byte-identical copies in the example's assets; sums are value hashes, not struct bytes.
19. **Limits** as the spec, plus ≤ 64 retired ids per struct and keys starting with a letter.
20. **AC-8 targets kept as written**; the store median flagged tight.
21. **Open question "real power-cut test"**: optional, once, recorded in the spec if done; not part of stage 10.
22. **Enum names** come from `NNH_DATA_ENUM` through spec 014's registry (links `schema_registry.o` only).
23. **Example controls and layout** as above (Old 3DS buttons, touch for every slot action); `settings.cpp` shows the
    "declarations in one source" pattern, `progress.hpp` the header pattern.
24. **Console stage**: three scripted runs (saves `check.nnhs` with the bench; actions `check.nnhs` for `NNHB`; data
    `check.nnhs` for D29).
25. **A fuzz target `fuzz_saves`** with its own header (`fuzz_targets.hpp` untouched): payloads as given and inside an
    `NNHS` file with the CRC and size fixed, every accepted file re-encoded and decoded again to the same values, random
    `OldFields` paths, `dump_bytes` in Debug.

## Constitution Check (after design)

| Article | OK? | Note |
|---------|-----|------|
| I | ✅ | Offsets per compiler, values as bits, a fixed byte order and field order, integer exactness tests; goldens on every host compiler and the console; sums over values |
| II | ✅ | No allocation; caller buffers of a compile-time size; stack 0.6–0.9 KB checked by `-fstack-usage`; costs estimated per workload (store median tight) with AC-8's fallback |
| III | ✅ | One header over `<nnh/data.hpp>`; constexpr tables; wrappers let other files see only the struct; measured |
| IV | ✅ | No events |
| V | ✅ | No heap, `virtual`, RTTI or exceptions; function-pointer steps |
| VI | ✅ | Tests first with hand-written bytes; goldens; power cuts at every operation; fuzz; negative-compile ctests; mutation pass |
| VII | ✅ | One module of 12 small sources, four detail headers |
| VIII | ✅ | API page, linked pages, example, D45/D46 |
| IX | ✅ | Own code, prior art cited, original shapes |
| X | ✅ | Bounded schema-free validator before any write to the struct, fuzzed; backup on bad structure; atomic writes unchanged and cut at every step on the fake |
| XI | ✅ | Optional module; no data loader or index linked; raw saves and `NNHB` byte-identical; `hello` unchanged |

No constitution exception is requested. D45 and D46 are new (see [decisions.md](../../docs/architecture/decisions.md)).
D29 is **not** amended (its traits and value types are reused read-only; `FieldKind` unchanged). D18 is not amended:
spec 002's save procedure is reused as is, with one appended internal accessor. The patterns page gains a "Versioned
schema" row. Flagged for the maintainer: the compile-time chain check (decision 6), `std::getenv` in core (decision 15),
`schema_registry.o` for enum names (decision 22), the store median (decision 20).

## Stage 0 results

Measured 2026-11-24 on the maintainer's Windows machine (WinLibs GCC 15, devkitARM), in the `spec-027` worktree at
`bafd9f2`, before any spec 027 change. Scripts in `build/ref-027/` (not committed). Other agents were building on the
same PC at the same time: timings are noisy.

- **T0.1** `hello` sizes (`size`: text / data / bss, file bytes): pc-release `hello.exe` 130 668 / 22 020 / 211 696
  (278 856 bytes); pc-debug 198 688 / 26 756 / 214 352 (1 297 082); 3ds-release `hello.elf` 201 124 / 28 352 / 106 680
  (`.3dsx` 246 512 bytes); 3ds-dev 217 896 / 32 872 / 109 288 (`.3dsx` 268 012). `nm -C` lists saved
  (`sizes-0/hello-*.nm`). `hello` links no `nnh::save::write` (pc-release and 3ds-release `nm`): `save.o` is not in
  it, so the appended `save_read_one` can't reach it. `.text` of 3ds-release ELFs: `actions` 245 996, `data` 319 428,
  `foundation` 206 936. Edit `examples/data/main.cpp` → simulator (pc-debug `data`) 2.15, 1.87, 1.86 s; → `.3dsx`
  (3ds-release `data_3dsx`) 5.54, 5.45, 5.49 s. Clean `nnh_core` (pc-debug, parallel) 10.5 s. Header cost
  (`header_cost.sh`, one-line TU, `g++ -std=c++20 -fsyntax-only`, best of 5): `<nnh/data.hpp>` 10 078 preprocessed
  lines, 0.43 s; `<nnh/fs.hpp>` 7 640 lines, 0.38 s. 3ds-dev disassembly of `save.o` and `save_format.o` saved in
  `dis0/`.
- **T0.2** Pins (`tests/saves/test_raw_unchanged.cpp`, values from the untouched spec 002 and 017 code): a 100-byte
  blob (`i * 7 + 3`) written by `nnh::save::write` with version 7 is a 116-byte file of CRC-32 `0x7E97C64D`; the
  bindings blob of a four-action set (spec 017's test set, Jump rebound to Y) saved with `save_to(..., 5)` is 28 bytes
  of CRC-32 `0xE82EA36E`; the fake's call sequence of a `save::write` over an existing save is `WSXNN` (write temp,
  size, remove backup, two renames) and of a `save::read` `SRR`; NNH Link's `link.sav` goes through the untouched
  `save::write`/`read` with its own `Storage::kFormat`.
- **T0.3** ctest pc-debug 271/271 (`data check: PASS checksum 0xD5BD3020`, `camera: PASS 5201E6CF`, `queries: PASS
  4ACAA177`, `entities: PASS C8FB9A41`, `prefabs: PASS 7BB7901E`, `hierarchy: PASS 23DB5C36`, `tweens: PASS B44CA3F6`,
  `platformer_replay_matches_script`), pc-release 259/259; `actions/tests/check.nnhs --pc` → `actions: saved 40 bytes
  sum AAB4AA36`, `actions: PASS 7/7`, `nnh: PASS (77 lines)`. `nnh_tests` 2440 tests against `kMaxTests` 4096: 1656
  left, no change needed.

## Stage 1 refinements and results

- **Files** as planned: `src/saves/save_wire.hpp` (wire byte, canonical varints, widths, little-endian loads and
  stores, `take_varint` for validated data), `save_header.{hpp,cpp}`, `save_validate.{hpp,cpp}`, and
  `save_report.cpp` (for now `FieldPath::format`, which the validator's messages use). `<nnh/saves.hpp>` starts with
  the limits, `WireKind`, `WireShape`, `FieldPath` and `path()` (the rest in stage 2). `FieldPath` gained `valid()`
  (a path that grew past 16 steps, or a negative item index, names nothing) and engine-use `push`/`pop`.
- **Messages are at most 63 characters** (`nnh::Error`), so the raw-file refusal is "a raw nnh::save file (read it
  with save::read)" (the file layer prefixes the path), and the newer-format one "format 2; this engine reads 1". The
  validator's reasons are `field <path>: <what>`; when the id itself is the fault the path ends in `?` (`field 4.?:
  bad id`). Its wording: bad wire byte 0xNN, bad id, id not ascending, value / string cut short, string is not UTF-8,
  string holds a NUL, bad length, struct length exceeds the file, nesting deeper than 8 levels, bad count, list count
  N exceeds the file, length N exceeds the file, items don't fill their length, template of N fields, template: bad
  wire byte / bad id / bad size, template ids not ascending, template cut short.
- **Depth**: the root's fields are level 1, a struct value's fields one level more, the rows of a struct sequence one
  level more than the sequence's field; an empty struct list is accepted whatever its template holds (no level is
  entered). A key with a NUL among its bytes is refused (`bad key`), so a key compare can't stop early.
- **Fake platform** (insertions as planned, plus one): `cut_power_after(ops, torn)`, `restore_power()`,
  `truncate_file()`, and **`fs_log()` / `clear_fs_log()`** — one letter per contract call (S size, R read, W write, A
  append, N rename, X remove) — so the tests compare call *sequences* (T0.2's "counts per function"); `count_call()`
  takes the letter, which touches each fs function's first line. A cut append keeps half its bytes when torn.
- **T1.3** `detail::save_read_one` appended to `save.cpp` (it calls the anonymous `read_one`; GCC doesn't inline it).
  3ds-dev `save.o`: every existing function byte-identical to `dis0/` (only the new section added); `save_format.o`
  identical.
- Tests: `test_wire.cpp` (9), `test_header.cpp` (6), `test_validate.cpp` (15: every one-value kind and every sequence
  shape accepted; every bound both ways with hand-written bytes; the two messages of the spec; eight levels accepted
  and nine refused, rows counted as a level; a 256 KiB adversarial file of 128-field rows validated in ≤ 2 steps per
  byte), `test_raw_unchanged.cpp` (5), `tests/platform_fake/test_power_cut_fake.cpp` (8).
- **T1.5** ctest pc-debug 271/271, pc-release 259/259; `nnh_tests` 2483 (pc-debug), 2443 (pc-release); pc-debug,
  pc-release, 3ds-release and 3ds-dev without a warning; spec 014's data module untouched (`git diff --exit-code`);
  `hello` sizes and `nm` lists equal T0.1's; `actions --pc` PASS 7/7 with `saved 40 bytes sum AAB4AA36`.

## Stage 2 refinements and results

- **Files** as planned: `<nnh/saves.hpp>` (limits, `WireKind`, `WireShape`, `FieldPath`, `path()`, `Report`, `FileInfo`,
  `OldFields`, `max_size`, `Buffer`, `encode`/`decode`/`store`/`load`/`info`/`exists`/`remove`, the dump calls, and a
  short `nnh::detail` block declaring the non-template functions they call), `nnh/detail/save_schema.hpp`
  (`SaveFieldInfo`, `SaveStep`, `SaveRole`, `SaveTypeInfo`, `SaveTag`, the `save_error_*` functions),
  `save_kinds.hpp`, `save_tables.hpp`, `save_macros.hpp`. One more source: **`src/saves/save_enum.cpp`**
  (`save_find_enum_names`, the only caller of spec 014's `find_enum`): an enum field's record points at a template
  `save_enum_names_of<E>` that calls it, so only a game whose save has an enum field links it and, through it,
  `schema_registry.o` (plan decision 22).
- **Records**: `SaveFieldInfo` is 32 bytes on the 3DS (name, nested record, enum lookup, offset, stride, items offset,
  count, id, capacity, kind, shape, width); `width` is the member's size for numbers, bools and enums (an enum's file
  kind is its underlying type's), 0 otherwise. `SaveTypeInfo` keeps three sizes per struct: `max_fields` (its fields
  with tags), `max_row` (a row of a sequence: values only) and `template_size`, so `max_size<T>()` is
  `16 + key length + max_fields` at compile time.
- **Checks** (all at compile time): as planned, plus a save struct needs at least one field
  (`save_error_a_save_struct_needs_a_field`), an enum whose underlying type is `char`, `wchar_t` or `bool` is refused
  (char's sign differs between systems), and **the step chain is complete from version 1**: every version 1 … c − 1
  needs exactly one step, so a version-3 type without steps doesn't compile (decision 6's strict reading; a game that
  bumps the version with nothing to convert writes an empty step). The macro parameter is `TypeName` (a parameter
  named `Type` replaced `SaveRole::Type` in the expansion).
- **Enum names in `nnh_tests`** come from the tests' data schema (`test_data::Element`, `NNH_DATA_ENUM` in
  `tests/data_assets/test_types.cpp`): a new `NNH_DATA_ENUM` in `nnh_tests` would change spec 014's registry tests
  (`count_enums == 1`, the pinned schema description). So the example's `difficulty_names.cpp` is linked by the
  example only; in `nnh_tests` the reference save's `Difficulty` has no names and takes any `std::uint8_t` (the golden
  values are valid enumerators, so the sums don't depend on it).
- Tests: `test_declare.cpp` (10: every one-value kind and sequence shape deduced; enums by their underlying type and
  their names looked up; fields sorted whatever the order; key, hash, version, role, depth; retired ids and steps in
  order; a summary; the global namespace and a spec 014 data type that is also a save type; `max_size` against hand
  sums, `Buffer` sizes; names only outside Release). Negative-compile ctests: `saves_declaration_accepts_good`
  (`-Werror`) and 27 cases × (`rejects`, `explains`) — unknown member, pointer, `std::string`, union, `char`, const
  member, id 0, id 65 536, an id twice, retired used, retired twice, nested without declaration, 129 fields, 9 levels,
  root over 64 KiB, array of 65 536, max size over 256 KiB (64 KiB of `u8` fields with 3-byte ids), keys upper case /
  17 characters / empty, version 0, a step at the current version, two steps for one version, a gap, a version in a
  struct, a summary stored, an undeclared type stored. Files generated once by `build/ref-027/make_compile_fail.py`.
- **T2.4** include check: `<nnh/saves.hpp>` pulls `nnh/data.hpp` (and its `assert`, `math`, `result`, `build`,
  `detail/data_*`, `type_id`, `printf_format`), `nnh/result.hpp` and its own four detail headers — no gfx, audio,
  tilemap or entities header. Header cost: 10 987 preprocessed lines and 0.461 s against `<nnh/data.hpp>`'s 10 078
  and 0.453 s: +0.008 s (budget +0.05 s; risk 6 not triggered).

## Stage 3 refinements and results

- **File** as planned: `src/saves/save_encode.cpp` (≈ 190 lines). Every write checks the room left first (`LimitReached`
  with nothing written past `capacity`, the message naming the size needed). A length prefix (a struct value, a string
  sequence's or a struct sequence's `bytes`) is written after its content: one byte is reserved and the content moved
  with one `memmove` when the length needs two or three bytes, so each value is encoded once (no size pass). A List's
  count is clamped to its capacity (memory is never trusted). Fixed-width sequences are one `memcpy` (stride = wire
  width for every fixed kind, little-endian), bool sequences one byte test per item. A string that isn't UTF-8 (a
  game that wrote raw bytes into a `data::String`) asserts in Debug/Development and returns `InvalidArgument`
  ("field 18: string is not UTF-8"; the encoder keeps a `FieldPath` for that message).
- **Overhead measured** (FR-5; hand bytes in the tests): a one-value field costs 2 bytes of tag (3 for ids ≥ 128, 4 for
  ≥ 16 384); a struct value +1 (+2 from 128 bytes of content); a string +1 (its length, not its capacity); a sequence
  of numbers +1–3 (count); of strings +2–6 (count, bytes); of structs +2–6 plus a template of 1 + 2 bytes per item
  field, once per sequence. The reference save's total is measured at stage 6.
- Tests: `test_encode.cpp` (8: every one-value kind against hand bytes — including three enums of `u8` and `i64`
  underlying types, a nested struct and every number at both signs —, a bool byte of 2 written as 1, defaults written,
  the id-length overheads, nested structs by their own ids, a short buffer at 0/10/21/half/full − 1 bytes with
  nothing written past it, strings of 0 to capacity bytes, a non-UTF-8 string), `test_encode_sequences.cpp` (5: every
  sequence shape by hand, empty sequences with their template, an array of structs inside list rows, two-byte counts
  and lengths, `max_size<T>()` equal to the encoded size of a full value for 10 shapes — including a 255 × 255-byte
  string list (65 kB) and a three-byte string `bytes` length).
- **T3.3** ctest pc-debug 326/326, pc-release; four presets without a warning; spec 014 untouched; `hello` unchanged.

## Stage 4 refinements and results

- **Files** as planned (`save_apply.{hpp,cpp}`, `save_convert.{hpp,cpp}`, `save_decode.cpp`, `save_migrate.cpp`,
  `old_fields.cpp`), plus two internal headers: `save_cursor.hpp` (skipping a validated value and reading a sequence's
  header, shared by the decoder, the view and the dump) and `save_internal.hpp` (`save_decode_payload`,
  `save_run_steps`, and the stage 5 hooks). The `saves` log channel is defined in `save_report.cpp`.
- **Decode order**: header (raw payload, format) → whole-structure validation → key (hash and text) → version → `T{}`
  → fields → steps. So a damaged file is `Corrupted` before its key is looked at (stage 5 tries the backup for it).
- **Merge by id**: a struct's file ids and its sorted fields and retired ids are walked together once (`Merge`), the
  same code for tagged fields and for rows (whose ids come from the template); a row is merged per row, never through
  a lookup table on the stack. **Lists** start from their item defaults: slots `[0, max(old count, n))` are reset
  (`T{}` per struct item, zero for the others) before the file's items are applied, so a list item missing a field
  gets the item struct's default, and freed slots keep the List invariant. **Arrays** keep `T{}`'s slot values as the
  base (an array with fewer items keeps the defaults for the rest).
- **Conversions** (`save_convert.cpp`, integer arithmetic only, `<bit>`'s `countl_zero`/`countr_zero`): as D46. A whole
  sequence whose kind can't convert at all (string ↔ non-string, struct ↔ non-struct) is one drop; scalar sequences
  drop per item. Same-kind integer, Color and reference sequences are one `memcpy`; float, Vec2 and bool sequences are
  checked per item (finiteness, 0/1), enum sequences per item only when the enum has names.
- **Enum names** are looked up once per run of the same field (`SaveApply::enum_names` keeps the last lookup), so the
  rows of a list search the registry once (risk item 6 of the ARM plan).
- **`OldFields::get`** converts into a temporary `T value{}` and copies it to `out` on success (so `out` is unchanged on
  failure, and a struct, array or list starts from its defaults); a string longer than the target is cut (true), a
  sequence into one value or the reverse is false. `old_fields.cpp` locates a path as a tagged value, an item, or a
  row (template + row bytes), then converts with `apply_field`/`apply_row`.
- **Missing step at run time**: the runner finds step v at index v − 1 (complete chain); a hand-built record with a null
  step asserts in Debug/Development and skips it in Release (`migrated` then counts only the steps run).
- Tests: `test_round_trip.cpp` (6), `test_evolution.cpp` (9), `test_fit.cpp` (9; the saturation test decodes 65 535 u16
  of 300 plus one more into u8 fields: 65 536 drops reported as 65 535), `test_versions.cpp` (6),
  `test_migrations.cpp` (5), `test_old_fields.cpp` (6), `test_summary.cpp` (4); `migration_types.hpp` (three versions
  of one type with call-logging steps, and a summary). Enum names in tests: `test_data::Element` (stage 2 note).

## Stage 5 refinements and results

Rebased on `main` at the start of the stage (specs 022 and 025 merged there): no conflict.

- **Files** as planned: `save_file.cpp` (`store`, `load`, `info`, `exists`, `remove`), the warning line in
  `save_report.cpp`, `save_dump.cpp`, `save_dump_env.cpp`.
- **Backup rules** (D46): a load tries `<path>.bak` when the main file is missing, unreadable (`IoError`), fails spec
  002's checks (length, CRC) or fails the structural validation (`Corrupted`); never for another key, a newer version or
  format, a raw file, or a buffer too small (`LimitReached`). When the backup is itself a save of another type or a raw
  file, that error is returned (it names the backup file). Main and backup both missing: `NotFound`; otherwise both
  unusable: `Corrupted` ("slot1.sav and its backup are unusable"), with one warning line giving both reasons.
  Structured messages name the file (`mig.sav: field 7: list count 900 exceeds the file`; spec 002's read messages
  already do); all are cut at 63 characters (`nnh::Error`).
- **Warning line** (one per load that used the backup, dropped or adjusted): `save <path>: backup used (<main's
  reason>)` or `save <path>: loaded`, then `; v1 -> v3 (2 steps)` when steps ran, `; unknown N (paths)`, `; dropped N
  (paths)`, `; adjusted N (paths)` — the unknown count is in the line too (it was not in the plan's example), paths only
  in Debug/Development. A load with nothing to report logs nothing (a migration alone included).
- **`info`** reads `fs_size` + one `fs_read` of up to 48 bytes, checks the `NNHS` header and the file length, then the
  structured header (no CRC); a missing or unusable main file gives its backup's header (`from_backup`); a raw file is
  `Unsupported`. **`exists`** is false for a bad path. **`remove`** removes each of the three files that exists, then
  `IoError` when one is still there.
- **Dump format** (Debug/Development): first line `save '<key>' v<version> (<bytes> bytes)`; a field `name (id) kind =
  value` (`kind` is the file's wire kind: `bool u8 i8 u16 i16 u32 i32 u64 i64 f32 f64 vec2 color ref texture sound
  music string struct`); a struct `name (id) struct` with its fields two spaces deeper; a sequence `name (id) u16[3]`
  (array) or `name (id) struct list[64]` (list), then items two spaces deeper — numbers 16 per line for 1-byte kinds,
  4 for 8-byte kinds, 8 otherwise (`[0] 1 2 3`), strings one per line, struct rows as `[i]` with their values four
  spaces deeper; strings quoted with `\"`, `\` and `\xNN` for bytes below 0x20 and from 0x7F (UTF-8 included, so the
  text is ASCII); floats `%.9g [bits]`, doubles `%.17g [bits]`, vec2 `(x, y) [bits bits]`, colors `#RRGGBBAA`,
  references `0xXXXXXXXX`; enums with `NNH_DATA_ENUM` names `5 (Fire)`; ids the schema doesn't know `#3 u32 = 120`,
  retired ones `#3 (retired) u32 = 120` (values shown: a dump says what the file holds). A schema of another key is
  ignored (ids only). Lines are built in a 600-byte buffer (`log_lines` cuts at the log's 160). Release:
  `dump`/`dump_bytes`/`dump_file` return `Unsupported` and `save_dump.cpp` compiles none of the dump.
- **`NNH_SAVE_DUMP`**: read once with `std::getenv` (any value but empty or `0`), overridable by
  `detail::save_set_dump_override(-1|0|1)` (tests); the text goes out through a 4 KB static buffer, `fs_write` for the
  first piece and `fs_append` for the next.
- Tests: `test_file.cpp` (7: the `NNHS` file around the payload, CRC included; `WSN` for a first store and `WSXNN`
  over a save — T0.2's raw sequence; a load is `SRR`; `NotFound`; no line for a clean or migrated load; one line with
  `unknown 1` and `dropped 1 (5)`), `test_backup.cpp` (6), `test_slots.cpp` (7), `test_power_cut.cpp` (4: 0, 1 and 2
  previous saves × a cut after 0–4 mutating calls × torn or not, then a load and another store; the cut between the two
  renames loads the backup), `test_dump.cpp` (7 in Debug/Development: the exact text of a small value, unknown and
  retired ids, a schema-free dump, a damaged buffer, a file with its backup, `<path>.txt` through the override, a
  7 KB dump in two pieces; 1 in Release: `Unsupported`), `test_save_misuse.cpp` (6), `test_save_cost.cpp` (3: no
  allocation over encode, decode, info, exists, dump and a migrated load; 128 visits for a 128-field struct; 1 + n + 3n
  for n rows of three fields, generated by `build/ref-027/make_cost_test.py`), `test_blob.cpp` (1: an `ActionSet`'s
  `NNHB` in a `List<std::uint8_t, 256>`, stored, loaded, `load_from` gives the rebinding back).

## Stage 6 refinements and results

- **Reference types** as the plan's table, in `examples/saves/progress.hpp` (v3, with `Hero`, `Item`, `Quest`,
  `Difficulty`, `QuestState`, `Checkpoint`) and `progress_history.hpp` (`ProgressV1`, `ProgressV2` with their own
  `StatsV1/V2`, `ItemV1/V2`, `QuestV1`). `ProgressV2` needs its own step from v1 (the chain rule):
  `progress_v2_from_v1`, the same weapon-to-inventory conversion as `progress_from_v1` (one template in
  `progress_migrate.cpp`). Step 2 sets `gems = coins / 10` and splits `location` into `room`/`tile` when the file has it
  (a v1 file doesn't: they keep their defaults). `difficulty_names.cpp` (`NNH_DATA_ENUM` of `Difficulty` and
  `QuestState`) is the example's only; `nnh_tests` and `make_saves_golden` don't link it, so their dumps show no enum
  names (stage 2 note).
- **Reference values** (`reference.cpp`): one FNV-1a sequence seeded with 1; floats made from their bits (item 5's
  `wear` is the subnormal `0x00000123`); name "Zoë Rangers" (12 bytes); 32 items in v1, 64 in v2/v3; 20 quests; seed
  `0x1234567890`; play time 3 600 000 (v1's u32) and 5 000 000 000 (v2, v3); best time 123.456; v3 labels on every
  fourth item and notes on even quests (one with quotes, for the dump's escapes). `reference_v1_migrated()` is the
  hand-made v1 → v3 expectation; `reference_v3_b()` the check mode's second store.
- **Sizes** (smaller than the plan's estimates — the items' labels and quests' notes are short): v1 1769 bytes of
  payload (plan ≈ 2.5 KB), v2 3590 (≈ 4.0 KB), v3 3768 (≈ 4.2 KB); `max_size<Progress>()` 4789 (≈ 4.9 KB). Framing in v3
  (FR-5): header 24, tags 32, lengths and counts 23, the two templates 30, label and note lengths 84 — ≈ 190 bytes over
  ≈ 3580 bytes of values: **5.3 %** (target < 25 %).
- **Golden files** (`tests/data/saves/`, `README.md` with how and why): v1.sav 1785 bytes (payload CRC `692F89F1`),
  v2.sav 3606 (`BB78EEE1`), v3.sav 3784 (`11363B5C`); loaded by the v3 code their `progress_sum` is `0F3BA8C8` (2 steps),
  `1EFDB382` (1 step), `D15774A7` (no step); unknown, dropped and adjusted 0 each. `.gitattributes` gains `*.sav binary`
  (one line; the `.txt` dumps are LF text). Copies in `examples/saves/assets/golden/`; ctest `saves_golden_copies_match`
  (a `compare_golden.cmake` loop) placed after `queries_self_check_pc`'s block. `make_saves_golden` (EXCLUDE_FROM_ALL)
  writes whole `NNHS` files with stdio (a host tool) and the dumps.
- **Fuzz** (`tests/fuzz/fuzz_saves.{hpp,cpp}`, `libfuzzer_saves.cpp`, `test_fuzz_saves.cpp`): a reference type with
  every kind and shape (ids 1–30, two retired, nested lists inside rows); each input is decoded as a payload as given,
  then as fields behind a valid header, then as a whole file through `load` on the fake (size and CRC fixed); accepted
  values must re-encode stably; the view runs on paths taken from the input; the dump runs (Debug/Development). The
  mini-fuzz (2 seeds + the 3 goldens × 3000 mutants) accepts 1340 of 15 000 as given (pc-debug).
- Tests: `test_reference.cpp` (5), `test_golden.cpp` (4; 3 in Release), `test_fuzz_saves.cpp` (2).

## Stage 7 refinements and results

- **Files**: as planned (`main.cpp`, `demos.hpp`, `summary.hpp`, `settings.{hpp,cpp}`, `tiny_game.{hpp,cpp}`,
  `slots_panel.{hpp,cpp}`, `hud.{hpp,cpp}`, `showcase_scene.{hpp,cpp}`, `check_scene.{hpp,cpp}`, `check_golden.hpp`,
  `bench.{hpp,cpp}`, `bench_scene.{hpp,cpp}`, `CMakeLists.txt`, `tests/check.nnhs`) plus `slot_files.{hpp,cpp}` (slot
  paths, `damage_save`, `install_golden`, a CRC-32 for the encode line: only public calls). **Damage** keeps the backup:
  the broken payload is written through a scratch `nnh::save::write` (a valid CRC), read back whole and put in place of
  the main file only with `fs::write_file`. The tiny game's **play time counts frames spent moving**, so the byte
  counts and sums the script expects don't depend on how many frames the PC or the console took between commands.
  The **reference values fill structs in place** (`fill_reference_v3(Progress&)` etc.; by-value wrappers for the
  tests): a `Progress` is about 5 KB, never a temporary on the 3DS main thread's stack (the scenes live on the heap).
  The check mode also removes `settings.sav` first, so two runs log the same settings line. The bench mutes log lines
  below Error while a workload runs (a backup load logs a warning by design: the bench times the save, not the
  remote log). `difficulty_names.cpp` registers `Difficulty` and `QuestState` (the example's dumps show
  `difficulty (11) u8 = 1 (Normal)`).
- **Check lines** (pc-debug and pc-dev; `check_golden.hpp`): `saves: check started`, `saves: v1 sum 0F3BA8C8 migrated
  2`, `saves: v2 sum 1EFDB382 migrated 1`, `saves: v3 sum D15774A7 migrated 0`, `saves: encode crc 11363B5C bytes 3768`,
  `saves: slot sum F6C8D12F`, `saves: backup used sum D15774A7`, `saves: summaries L300 empty empty`, `saves: removed
  slot1 exists 0`, `saves: PASS`. With `slot_sum` set to 0 the check logged `saves: FAIL slot sum F6C8D12F` (checked
  once). **Showcase lines** of the script: `saves: showcase slots empty empty empty`; `slot 1: stored 2206 bytes` (twice:
  before and after `hold RIGHT 40`), `slot 2: selected`, `slot 2: stored 2206 bytes`, `name: Zoë`, `slot 3: selected`,
  `slot 3: stored 2207 bytes`; `golden v1: loaded sum 0F3BA8C8 steps 2`; `slot 1: damaged`, `slot 1: loaded sum 4B109E8C
  version 3 steps 0 unknown 0 dropped 0 adjusted 0 backup`; `slot 2: deleted`; `settings: volume 8 language English
  stored`; then the bench lines.
- **Screenshots** (`SAVES_SHOT=empty|saved|golden|backup`, 30 frames, read back): empty slots; three slots with their
  summaries ("Ana L1", "Zoë L1 gems 12 1:06", "Bo L9"); golden v1 loaded ("Zoë Rangers level 37 gems 12345", 33 items,
  `v1 -> v3 steps 2`); a load from the backup (`backup used`, slot 1 marked "(backup)"). The report shows on two lines
  (one did not fit 400 pixels). `NNH_SAVE_DUMP=1` wrote `slot1.sav.txt` (read back: `save 'progress' v3 (2206 bytes)`,
  names, enum names).
- ctests `saves_self_check_pc` and `saves_check_script_pc` (`RESOURCE_LOCK nnh_remote_port;nnh_saves_data`) after the
  queries block; the script passed twice in a row (`nnh: PASS (114 lines)`).
- **PC bench** (pc-dev, three runs, other agents building on the same PC): encode 0.011–0.020 ms, decode 0.032–0.072
  ms, decode v1 0.015–0.036 ms, store median 3.8–5.8 ms (max 4.2–22.8), load 0.6–0.9 ms, backup 1.2–1.6 ms, summaries
  1.6–2.5 ms, raw write 4.0–5.3 ms; PASS.

## Stage 7 ARM pre-check

3ds-dev objects (`build/ref-027/armcheck.py`: per function the instructions, calls, VFP and `pld`;
`stackcheck.py`: `-fstack-usage`):

- **No FPU arithmetic in the codec**: the only VFP instructions in `save_apply.o` and `save_convert.o` are `vldr`/`vstr`
  of one 8-byte constant (a struct zeroed through a d-register: a bit move, no conversion, no flush). `vcvt` appears
  only in the dump's `printf` arguments (Debug/Development). **No `__aeabi_*` in the codec** (`__aeabi_uidivmod` is in
  the dump's line breaking). `__ctzdi2` (64-bit `countr_zero`, a libgcc call) was in the conversions: replaced by
  32-bit halves. `memcpy` only for packed sequences, strings and 8-byte loads; `memmove` only in the encoder's
  length prefixes. `pld` added to the validator's field and row walks (`__builtin_prefetch`, 64 bytes ahead).
- **Hot paths rebuilt without a byte changing** (goldens hold): same-kind bool and float sequences are checked and
  copied inline (the 1024 explored flags were 1024 calls of `convert_scalar`); enum names are looked up once per run of
  rows.
- **Functions** (instructions, whole function): `put_fields` 107, `put_one` 124, `put_sequence` 448, `put_string` 82;
  `Walk::fields` 127, `Walk::one` 126, `Walk::sequence` 520 (strings and rows inlined); `apply_fields` 142,
  `apply_by_id` 89, `apply_field` 408 (`apply_one`/`apply_sequence` inlined), `apply_row` 153, `convert_scalar` 387,
  `convert_string` 37; `locate` 478 (old fields).
- **Estimates per AC-8 workload** (executed instructions × 1.3 plus about 110 cycles per 32-byte line missed, 268 MHz):
  encode about 40 k instructions (640 row values × 45 with the path stack, 1024 flags × 6, 84 strings) + 280 lines
  (5 KB read, 3.8 KB written), 83 k cycles: **about 0.31 ms** (< 0.5); decode: validation 25 k (the flag and bestiary
  arrays are O(1) there), `T{}` 1.3 k stores, apply 75 k (640 values × 100, flags inline, bestiary one copy), × 1.3 +
  400 lines, 175 k cycles: **about 0.65 ms** (< 1 ms; above the plan's 0.45–0.55); decode v1 (1.8 KB, 32 items, 3 view
  lookups, a 33-item shift), 100 k cycles: **about 0.38 ms** (< 1.2). The card workloads are unchanged from the plan's
  table (store 55–105 ms, 137 with margin: tight; load 10–25; backup 20–50; summaries 30–75).
- **Stack** (3ds-dev frames): `save_load` 704, `load_one` 240, `save_decode_payload` 208, `save_validate_fields` 136,
  `Walk::fields` 56 + `Walk::sequence` 64 per level, `apply_fields` 56 + `apply_by_id` 40 + `apply_field` 80 +
  `apply_row` 64 per level, `save_run_steps` 40, `old_fields_get` 104 (its scratch report is static), `locate` 128,
  `save_decode` 464. A file load of the three-level reference save peaks at **about 1.5 KB** (about 2.9 KB at 8
  levels); a memory decode about 1.05 KB. **The plan's < 1 KiB target is missed**: a `Report` (350 bytes: 9 paths of 36
  bytes) is returned inside a `Result`, whose constructor takes its value by copy (spec 002's `result.hpp`, not
  changed); the internal calls already share one report (the first build measured 984 + 528 + 872 bytes for the same
  three frames). Flagged for the maintainer; the 3DS main thread's stack (32 KB by default) has room.

## Stage 10: console run (prepared)

T10.1 on the PC: `scripts/dkp.sh cmake --build --preset 3ds-release` and `scripts/dkp.sh cmake --build --preset 3ds-dev
--target saves_3dsx actions_3dsx data_3dsx` without a warning; 3ds-release `hello.elf` equal to T0.1's (the stage-end
guards). On the Old 3DS, each run: Homebrew Launcher, press **Y**, nobody touches the console after.

1. `build/pc-debug/tools/nnh/nnh run saves --script examples/saves/tests/check.nnhs` (about 1 min): every line of
   "Stage 7 refinements and results" equal to the PC's (the sums, `encode crc 11363B5C bytes 3768`, the byte counts
   2206/2207, `loaded sum 4B109E8C … backup`, `settings: volume 8 language English stored`), then the bench lines and
   `saves bench: PASS` (expected: encode 0.2–0.4 ms, decode 0.5–0.8, decode v1 0.3–0.5, store median 55–140 ms — the
   most likely FAIL —, load 10–35, backup 20–65, summaries 30–100). Screenshots `check`, `empty`, `saved`, `golden`,
   `backup`, `bench` in `build/logs/` read back. A FAIL fails the run after every number is logged: record them in
   "Console run 1" and apply T10.3.
2. `build/pc-debug/tools/nnh/nnh run actions --script examples/actions/tests/check.nnhs` (about 40 s): `actions: saved
   40 bytes sum AAB4AA36`, `actions: PASS 7/7`, `nnh: PASS`.
3. `build/pc-debug/tools/nnh/nnh run data --script examples/data/tests/check.nnhs` (about 20 s): `data check: PASS
   checksum 0xD5BD3020 (4 enemies, 3 items, 1 settings, index 595 bytes)`, `nnh: PASS`.

Optional (decision 21): copy `slot1.sav` from `sdmc:/3ds/saves/` into the simulator's data folder
(`build/pc-debug/examples/saves/data/saves/`) and load it there (the same summary).

## Mutation testing

`build/ref-027/mutants.py` (88 hand mutants: wire, header, validator, encoder, conversions, apply, decode, steps, view,
file layer, report, dump file, 15 compile-time checks run against the negative-compile ctests, the fake's power cut),
applied one at a time by `mutate.py` (pc-debug, whole `nnh_tests` or `ctest -R saves_declaration`). First pass: 15
survivors (and 5 patterns to fix): a sequence one byte short, rows that can't fit (caught later with another message),
a List count above capacity in memory, signed limits (`<` vs `<=`), the float exponent limit (2^128), list slots past
the file's items with a default list, a bad bool byte inside a bool array, two keys of one FNV-1a hash ("bgpx_" and
"b13ck"), a string list's item skip, a row read into a number, a damaged main file without backup (Corrupted, not
NotFound), `info` on a cut file whose headers are whole, a load that only adjusted, `NNH_SAVE_VERSION` twice. Each got
a test (`tests/saves/test_edges.cpp`, 13 tests) or a negative-compile case (`version_twice`). Second pass: **87 of 88
killed**; 1 equivalent (`O1`: the view's early stop when the file's ids pass the one looked for is only a shortcut).
One mutant of the first list was equivalent too (a path-count condition) and was replaced. Score 87/87 non-equivalent.

## Stage 8 results

- **T8.2** `check_release_strings.sh`: the dump's texts, `NNH_SAVE_DUMP` and two of the example's member names; checked
  on `saves.3dsx` and pc-release `saves.exe` (ok) and on pc-debug `saves.exe` (FAIL, the positive control). A 3DS `.elf`
  keeps DWARF (every struct member's name), so the names are checked on the `.3dsx`.
- **T8.3** Probes (`build/ref-027/probe_*.cpp`, linked against pc-release `libnnh_core.a` with a map file): a save
  without enums links no `src/data` object; with an enum field, only `save_enum.o` and `schema_registry.o`. The 3ds-release
  `saves.elf` has no `data_load`, `data_index`, `index_validate` or `validate_data` symbol. `hello` sizes and `nm` equal
  T0.1's at every stage. Saves module, 3ds-release objects (text + rodata): validate 4.0 KB, encode 3.5, file 3.4, apply
  3.2, old fields 3.0, convert 1.8, report 1.5, header 1.2, decode 0.8, the rest < 0.3: ≈ 22.6 KB (the plan estimated
  7–10 KB for the codec alone; the codec here is ≈ 14.6 KB).
- **T8.4** Console scripts with `--pc`: actions (`saved 40 bytes sum AAB4AA36`, `PASS 7/7`), animation, camera, data,
  entities, foundation `assert_crash` PASS; hierarchy, prefabs and saves PASS as ctests (`*_check_script_pc`), the
  platformer replay ctest passes; the run of the remaining scripts (foundation `cpu_fault`, input, pathfinding,
  platformer gate/level/zoom_gate, queries, tweens) was cut by the session stop: for the lead's post-merge run. Edit
  `examples/saves/tiny_game.cpp` → simulator 1.86–1.94 s, `.3dsx` 3.76–3.89 s; edit `progress.hpp` (the declarations'
  header, 7 sources) → 2.26–2.28 s, 5.38–5.53 s (constitution III: < 2 s / < 5 s for one game `.cpp`; the header edit
  rebuilds several sources). `.text` of `actions`, `data`, `foundation` unchanged (none links saves).
- Guards: ctest pc-debug 333/333, pc-release 317/317; `nnh_tests` 2745 (pc-debug), 2698 (pc-release); four presets
  without a warning; spec 014 untouched; `hello` unchanged.

## Stage 11

Docs: `docs/api/saves.md` (new), `fs.md`, `data.md`, `architecture/security.md`, `decisions.md` (D45/D46
implemented), `patterns.md`, `REFERENCES.md`, `CLAUDE.md`, `ROADMAP.md`, `specs/README.md`, statuses. Stage 9 (Linux,
VM 120) and stage 10 (console) are the lead's and the maintainer's; T11.7 (merge) is the lead's.

## Maintainer decisions after the implementation (2026-11-24)

- **Stack:** the ≈ 1.5 KB file-load peak (≈ 2.9 KB at 8 levels) is accepted; the < 1 KiB target is retired. The 3DS
  main thread's stack is 32 KB by default, and lowering it would mean changing spec 002's `Result` (used by every
  module) or the load API. `docs/api/saves.md` states the measured stack use.
- **Code size:** the module's ≈ 22.6 KB of 3ds-release `.text` (codec ≈ 14.6 KB, against the 7–10 KB estimate) is
  accepted: the module is opt-in (`hello` is byte-identical), so only games that use structured saves pay it.

## Stage 9 results (T9.1)

On `nnhge-build` (VM 120, Ubuntu, 4 vCPU), 2026-11-25, `main` at the D47 and probe fixes (specs 026a and 027 together):

- **GCC 13** (pc-debug): `nnh_tests` 3 200 tests, 0 failures; ctest 331 of 341 at first — the 10 `*_script_pc` tests
  need the `pc-dev` preset's folder (`build/pc-dev`), which `-B` builds don't make; with `pc-dev` built (GCC 13) 10 of 13
  pass and the 3 left (`transitions_check_script_pc`, `particles_check_script_pc`, `platformer_doors_script_pc`) fail
  only on their benches' timing verdicts (the VM draws with SDL's software renderer, no GPU: e.g. a fade 1.29 ms against
  the console's 0.11 ms budget, a doors frame over 25 ms); every logic, self-check and golden line matched.
- **GCC 14** (pc-debug): `nnh_tests` 3 200, 0 failures; ctest without the scripts 328 of 328. One warning, fixed:
  `-Wformat-truncation` in `examples/models/check_scene.cpp` (a 128-byte text for up to 169 bytes; now 192).
- **clang 18 with ASan + UBSan** (`-fno-sanitize-recover=undefined`, `ASAN_OPTIONS=detect_leaks=1`): `nnh_tests` 3 200,
  0 failures, no sanitizer report. Only `nnh_tests` and the fuzzers were built with sanitizers: the VM's disk was full
  (`/var/lib/containerd` holds 9.8 GB of container images; not touched).
- **libFuzzer** (clang 18, ASan): `fuzz_saves` 600 s, 3 061 285 runs, cov 962; `fuzz_save` 300 s, 11 636 185 runs;
  `fuzz_bindings` 300 s, 282 734 runs; `fuzz_data` 300 s, 4 145 137 runs; `fuzz_assetc_gltf` 600 s, 1 148 342 runs, cov
  1128; `fuzz_mesh_reader` 600 s, 23 599 172 runs — no crash, leak or timeout in any.
- **AC-2 on every compiler**: the v1/v2/v3 sums, `encode crc 11363B5C bytes 3768`, the slot and backup lines and the
  golden files in `tests/data/saves/` are checked by `nnh_tests` and the `--pc` script (GCC 13): identical everywhere.
- **32-bit host build**: not done — the VM has no 32-bit multilib (`gcc-13 -m32` finds no `crti.o`); installing it is a
  system change for the maintainer.

## Console run 1 (T10.2, 2026-11-25, Old 3DS XL, Development build)

Scripts own the controls (spec 013's amendment). Logs: `build/logs/saves-20261125-104918.txt`, the `actions` and `data`
runs right after.

- `check.nnhs`: `saves: v1 sum 0F3BA8C8 migrated 2`, `v2 sum 1EFDB382 migrated 1`, `v3 sum D15774A7 migrated 0`,
  `encode crc 11363B5C bytes 3768`, `slot sum F6C8D12F`, `backup used sum D15774A7` (with the expected warning
  `backup used (slot1.sav: field ?: bad wire byte 0x7F)`), `summaries L300 empty empty`, `removed slot1 exists 0`,
  `saves: PASS` — equal to the PC's.
- Bench (old targets): `encode` avg **0.517 ms** (< 0.5: FAIL), `decode` avg **1.437 ms** (< 1: FAIL), `decode v1`
  0.676 ms (< 1.2: PASS), store median 128.9 ms, max 131.3 (< 150, max 300: PASS), load median 36.3 ms (< 50: PASS),
  backup median 72.5 ms (< 100: PASS), summaries median 109.5 ms (< 150: PASS), raw write median 2 154.4 ms (no target;
  logged). Encode and decode are about twice the ARM pre-check's estimates (0.31, 0.65): T10.3's optimisation round.
- `actions/tests/check.nnhs`: `actions: saved 40 bytes sum AAB4AA36`, `actions: PASS 7/7`, `nnh: PASS`.
- `data/tests/check.nnhs`: `data check: PASS checksum 0xD5BD3020 (4 enemies, 3 items, 1 settings, index 595 bytes)`.

## Optimisation round (T10.3)

**Console run 1** (Old 3DS XL, Development, 2026-11-25, as reported by the maintainer): every logic line equal to the
PC's (v1/v2/v3 sums, `encode crc 11363B5C bytes 3768`, backup, summaries, `saves: PASS`); bench (average of 100):
encode **0.517 ms** (target < 0.5, FAIL), decode **1.437 ms** (< 1, FAIL), decode v1 0.676 ms (< 1.2, PASS); store,
load, backup and summaries PASS. Both misses are about twice the stage 7 pre-check. Budgets and targets are unchanged
here (the maintainer sets them after the rerun).

**Method.** The pre-check estimated executed instructions from function sizes. This round counted them: an ARMv6
interpreter (`build/opt/armsim.py`, an analysis script outside the repository) loads the 3ds-dev `saves.elf`, runs
`SavesBench::start()` and then `encode()`, `decode()`, `decode_v1()` exactly as the bench calls them, and records per
function the executed instructions, loads, stores, registers moved by `ldm`/`stm`, taken branches and L1 misses
(16 KB, 4 ways, 32-byte lines, LRU; data and instructions; a cold run, then warm runs). Its encode gives
`crc 11363B5C bytes 3768`, and its decode returns the reference values. What the timed region holds was checked in
`bench_scene.cpp`: 10 runs per frame (the first one cold), the log level changes and the profiler scope outside the
loop; each run includes the `Result<Report>` (two 350-byte clears), the validation, `T{}` (four `memset`s, 7.8 KB) and
the 64 item resets of `open_list` — all small next to what follows.

| Before (per run) | instructions | stage 7 estimate (instructions / cycles) | `ldm`/`stm` registers | misses (cold / 10 + warm) | run 1 cycles | cycles per instruction |
|------------------|-------------:|-----------------------------------------:|----------------------:|--------------------------:|-------------:|-----------------------:|
| encode | 94 815 | ≈ 40 k / 83 k | 13 096 | ≈ 46 | 138.6 k | 1.40 |
| decode | 241 010 | ≈ 101 k / 175 k | 48 232 | ≈ 88 | 385.1 k | 1.56 |
| decode v1 | 118 184 | — / 100 k | 22 167 | ≈ 101 | 181.2 k | 1.44 |

(cycles per instruction after taking off the misses at 110 cycles each.) The misses were right (5–10 % of the time);
**the instruction counts were 2.4× too low**, and calls cost more than 1.3 cycles per instruction (9-register pushes
and pops per value). The drivers, from the per-function counts:

1. **Per-value machinery in rows** (decode): each item value went `apply_row` (the template's varint again) →
   `skip_value` → `apply_by_id` (the field merge and the retired list) → a path push and pop → `apply_field` (enum
   lookup, shape test) → `apply_one` → `convert_scalar` → two variable-size `memcpy` calls (`read_int` and the store):
   about 330 instructions per value. `apply_by_id` 48.9 k, `apply_field` 39.5 k, `convert_scalar` 34.6 k, `apply_row`
   24.0 k, `memcpy` 21.9 k (975 calls).
2. **Validator rows**: `Walk::value` → `Walk::one` with a path push and pop per value and the template re-read:
   `Walk::sequence` 34.6 k + `Walk::one` 18.3 k, about 70 per value (the estimate assumed about 25).
3. **Encoder values**: `put_value` → `put_one` → `Out::bytes` → a `memcpy` call per 1–8-byte value (586 calls) and a
   path push and pop per value: `put_sequence` 39.7 k, `put_one` 25.1 k, `memcpy` 16.1 k.
4. **newlib's `memcpy` copies byte by byte** when source and destination are differently aligned: the 1 KB bestiary
   (a packed `u16[512]` at an odd file offset) cost about 6 k each way.
5. **A length prefix moved 1.5 KB**: one byte was reserved before the inventory's rows, whose length needs two, so
   `close_length` moved them with `memmove` (6.2 k).

What changed (no output byte, value, count, path, step or message changed):

- `save_rows.{hpp,cpp}` (new): a struct sequence's template is matched with the item struct once — a plan of up to 16
  columns (6 bytes each): copy 1/2/4/8 bytes, bool check, `f32` check, string, convert (`apply_field`), unknown,
  retired — and each row is applied by its columns' actions. The row's path step, the notes in their order and
  `visited` are `apply_row`'s; longer templates use `apply_row`.
- `save_validate.cpp`: templates of up to 16 one-value fixed or string columns are walked as runs (consecutive fixed
  bytes, strings) without path steps; at a fault the row is walked again by the old code, which names it (same
  message, same steps). An empty string skips `utf8_valid` and `memchr`.
- `save_encode.cpp`: a row of one-value fixed and string columns is written without per-value room checks or path
  steps when the struct's `max_row` fits (the bad-text path is built from the row and id); fixed values are written
  by fixed-size copies; a length prefix reserves the varint size of the content's smallest length (rows: count × the
  smallest row; strings: the count), so the content moves only when that guess was short (the 20 quests, 143 bytes).
- `save_bulk.hpp` (new): `copy_bytes`, `copy_value`, `load_value`/`store_value`, `put_bools` (any non-zero byte → 1,
  four at a time) and `copy_bools` (words of 0/1 copied, stops at the first other byte): inline word loops with
  unaligned loads and stores (the ARM11 and x86 allow them) in place of `memcpy` calls; used by the packed copies and
  bool arrays both ways and by `convert_scalar`'s integers.

| After (per run) | instructions | change | `ldm`/`stm` registers | misses (cold / 10 + warm, LRU) |
|-----------------|-------------:|-------:|----------------------:|-------------------------------:|
| encode | 32 705 | −65 % | 1 292 | ≈ 48 |
| decode | 54 328 | −77 % | 4 308 | ≈ 135 (≈ 92 if the instruction cache replaces well) |
| decode v1 | 32 429 | −73 % | 2 971 | ≈ 210 (≈ 120) |

Largest functions after: decode `apply_rows` 16.8 k, `memset` 10.0 k (`T{}` and the item resets: the game's
constructors), `Walk::sequence` 6.9 k, `apply_field` 6.3 k (the 1 024 flags and the bestiary inline), `convert_scalar`
2.6 k; encode `put_sequence` 28.1 k (rows, flags, bestiary, strings inline), `utf8_valid` 1.5 k.

**Expected Old 3DS numbers** (cycles = instructions × 1.3–1.6 + misses × 110, 268 MHz; the same model brackets run 1:
0.48–0.59, 1.20–1.47 and 0.62–0.75 ms against 0.517, 1.437 and 0.676): **encode ≈ 0.19 ms (0.18–0.22)**, **decode ≈
0.36 ms (0.30–0.38)**, decode v1 ≈ 0.24 ms (0.21–0.28); the card workloads are unchanged (the codec is under 1 % of
them). Risks: the decoder's code is now 10.6 KB over 331 cache lines with 15 sets holding more than four (v1: 12 KB,
29 sets), so the warm instruction misses (65–150 per run with LRU, 7–17 k cycles) depend on the 3DS's replacement
policy — included in the upper ends; whether the data cache allocates on writes changes little (the reference data
fits in 16 KB).

**Costs.** Code: the module's 3ds-release `.text` + `.rodata` grows from 22.8 KB to 28.2 KB (`save_rows` 2.5 KB, encode
+1.8 KB, validate +0.5 KB, apply +0.4 KB, convert +0.1 KB). Stack: +112 bytes per level of struct rows while decoding
(`apply_rows` 216 bytes for `apply_row` 64 + `apply_by_id` 40), +32 per sequence level while validating, +48 while
encoding: a file load of the reference save peaks at about 1.6 KB (was 1.5 KB); 8 levels of struct rows about 3.7 KB
(was 2.9 KB).

**Tests.** First: `tests/saves/test_bulk.cpp` (6, seen failing to compile, then green) and `tests/saves/test_rows.cpp`
(14 characterisation tests of row behaviour, green on the old code: per-column values, report counts and paths per
row, values visited, a 17-column template, the validator's row messages and steps, a bad string in a row, bools of any
byte, struct values and bools in rows, every short buffer, canonical lengths around 128 and 16 384 bytes). Then
`nnh_tests save` 197/197 (pc-debug) and 190/190 (pc-release), `ctest -R saves` 60/60 (the self-check and check
script lines, the golden copies, the declaration cases), the mini-fuzz unchanged (1 340 of 15 000 mutants accepted),
3ds-dev `saves_3dsx` and 3ds-release builds without a warning. Hand mutants (`build/opt/mutate.py`): 25 of 25 killed —
the word helpers (second word, bool shift and mask, value widths, the high word), the plan (copy width, bool limit,
unchecked `f32`, cut string counted as dropped, unknown path, retired and unknown swapped, `visited`, the row index,
the string capacity), the bool-array drop index, the quick validator (runs merging strings, an unchecked run, steps,
the empty-string shortcut), the encoder (the reserved length size, flat rows without room, a struct column taken as
flat, an unnormalised row bool, the string-list reservation).

**Next:** T10.2 run 1 again on the Old 3DS (`build/pc-debug/tools/nnh/nnh run saves --script
examples/saves/tests/check.nnhs`): the same logic lines and the bench; then the budgets with ≈ 25 % margin.

## Console run 2 and budgets set (2026-11-25, Old 3DS XL, Development build)

After the T10.3 round. Log: `build/logs/saves-20261125-125724.txt` (the `opt-027` worktree). Every logic line equal to
run 1 and the PC's (`v1 sum 0F3BA8C8 migrated 2`, `v2 sum 1EFDB382`, `v3 sum D15774A7`, `encode crc 11363B5C bytes
3768`, `slot sum F6C8D12F`, `backup used sum D15774A7`, `summaries L300 empty empty`, `removed slot1 exists 0`,
`saves: PASS`); `saves bench: PASS`; `nnh: PASS`.

| Workload | Run 1 | Expected | Run 2 | Old target | Budget (run 2 + ≈ 25 %) |
|----------|-------|----------|-------|------------|--------------------------|
| encode (avg) | 0.517 | 0.18–0.22 | **0.203** | < 0.5 | **< 0.25 ms** |
| decode (avg) | 1.437 | 0.30–0.38 | **0.383** | < 1 | **< 0.48 ms** |
| decode v1 (avg) | 0.676 | 0.21–0.28 | **0.234** | < 1.2 | **< 0.29 ms** |
| store (median / max) | 128.9 / 131.3 | — | 129.8 / 131.5 | < 150, max 300 | **< 165 ms**, max 300 (the spec's limit) |
| load (median) | 36.3 | — | 35.4 | < 50 | **< 45 ms** |
| backup (median) | 72.5 | — | 71.7 | < 100 | **< 90 ms** |
| summaries (median) | 109.5 | — | 107.6 | < 150 | **< 135 ms** |
| raw write (median, no target) | 2 154.4 | — | 131.0 | — | — |

The codec now costs 2.5 to 3.8 times less, every number inside (decode at the top of) the round's range. The raw write's
2.2 s in run 1 was the card's (one slow write right after the stores; it repeats nothing of the codec). SD numbers are
this card's: another card may be slower or faster.
