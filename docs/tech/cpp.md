# C++ in NNHGE

C++20, used as a **predictable** language: you can tell what a line costs by reading it. These rules
apply to `engine/` and `tools/`. Games may use more of C++, but the same rules keep them fast on 3DS.

## Not in the engine

| Feature | Why | Instead |
|---------|-----|---------|
| Exceptions (`-fno-exceptions`) | Code size, hidden control flow on ARM11 | Return values: `bool`, `std::optional`, a small `Result`; `NNH_ASSERT` for programmer errors |
| RTTI (`-fno-rtti`): `typeid`, `dynamic_cast` | Code size | Compile-time type ids (template static address), explicit tags |
| Heap allocation in per-frame paths | Fragmentation, unpredictable time | Fixed-capacity arrays, pools, buffers sized at `init()` |
| `virtual` per object in hot loops | Cache misses, indirect calls | Arrays of plain structs + one function; `virtual` per scene/system is fine |
| `std::function` in hot paths | May allocate | Function pointers + context, or fixed-size callables |
| `<iostream>`, `<regex>`, `<locale>` | Big, slow to compile | `printf`-style, hand-written parsing |

**Classes games derive from** (e.g. `nnh::Scene`) define **every virtual function inline** in the public
header, destructor included. The engine is built without RTTI, games may not be: an out-of-line virtual
(the "key function") would make the engine the only place to emit the class's type info, and a game
built with RTTI then fails to link (`undefined reference to typeinfo for nnh::Scene` with GCC on Linux).

## Error handling

```cpp
// Recoverable (file missing, out of memory at load): return it.
std::optional<Texture> load_texture(const char* path);

// Programmer error (null handle, frame not open): assert, documented in the header.
NNH_VERIFY(handle.valid(), "texture used after destroy");   // see docs/api/assert.md
```

Public functions document: does it allocate? is it per-frame safe? what happens on misuse?

## Headers

- A public header includes only what its declarations need (`<cstdint>`, other `nnh/` headers). No
  platform headers (`<3ds.h>`, `<SDL3/SDL.h>`) in anything under `engine/core/include/`.
- Hide implementation: opaque handles, internal headers in `engine/core/src/`, or PIMPL. Changing an
  implementation must not recompile games.
- `#pragma once`. Include order: own header, standard library, third party, project (clang-format regroups).
- Braced options must compile on GCC 13 and 14 (found on the Linux build VM, specs 022 and 024):
  - No `= {}` on an array member whose element type has default member initialisers: the elements' defaults apply
    anyway. With it, GCC 13 and 14 refuse a braced `{.x = ...}` argument of that aggregate.
  - A function overloaded so that a braced options argument is tried against `const T&` needs `T` complete in the
    header (include it, don't forward-declare it). Otherwise GCC 13 crashes (`draw(screen, {.layer = n})` against
    `draw(screen, const Texture&, ...)`).
  - Lock each in with a `static_assert` over a `requires` expression, in a test that includes the header first.

## Types and style

- `std::uint32_t` & co. for sizes that matter (hardware, files); `int` for ordinary counts and pixels.
- `enum class` for named values; bit-flag enums (like `nnh::Button`) document that values are bits.
- `constexpr` for constants and pure helpers (`nnh::rgb`, `screen_width`).
- Aggregates with default member initializers + designated initializers for configs:
  `nnh::init({.title = "My Game"})`.
- `const` by default; references for non-null parameters; no owning raw pointers (RAII handles).
- No `using namespace` in headers. Anonymous namespaces for file-local helpers.
- Formatting: `.clang-format` (LLVM base, 4 spaces, 110 columns), applied with **clang-format 23** (older versions
  wrap a few constructs differently; nothing checks formatting automatically, so use 23 to avoid format-only diffs). Naming: see
  [project-structure.md](../architecture/project-structure.md#naming).

## Memory on 3DS

- Know who owns every buffer and which memory it lives in (heap, linear, VRAM — see
  [devkitpro-3ds.md](devkitpro-3ds.md)).
- Allocate at load time, reuse during play. Containers like `std::vector` are fine at load time,
  not in per-frame code.
- Watch what the C library pulls in: one `sscanf` added about **40 KB** to a 3DS `.3dsx` (the whole
  `scanf` family); `strtol`/`strtof` or a small hand-written parser cost almost nothing. Check the
  `.3dsx` size after adding parsing or formatting code.
- The same for the engine itself: anything `init()`/`frame()` calls directly is linked into **every**
  game. Calling music from the frame loop put the Ogg decoder (+60 KB) in `hello`; now music registers
  function pointers the first time a game uses it (`MusicHooks`, spec 008), so games without music
  don't link it. Measure `hello.3dsx` when the frame loop gains a call.

## Compile-time hygiene (build speed)

- Templates only where they pay off (e.g. typed `subscribe<T>`); keep their bodies small and in headers
  that few files include.
- Prefer forward declarations in headers; move heavy includes to `.cpp`.
- `nnh/nnh.hpp` (include-everything) is for quick experiments only, not for engine or example code.

## Further reading

- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
- Butano's design ([GValiente/butano](https://github.com/GValiente/butano)): C++ without heap/exceptions on GBA
- Mike Acton, *Data-Oriented Design and C++* (CppCon 2014)

## Compiler builtins instead of `<type_traits>` in light headers

`<nnh/live.hpp>` (spec 030) tests types with `__is_same(T, U)` and `__is_enum(T)` instead of `<type_traits>`, which
adds ≈ 3 200 preprocessed lines to every game file that includes it. GCC, Clang and MSVC all provide these builtins;
use them only in headers that must stay light, and only for these simple checks.
