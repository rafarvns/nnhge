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

## Error handling

```cpp
// Recoverable (file missing, out of memory at load): return it.
std::optional<Texture> load_texture(const char* path);

// Programmer error (null handle, frame not open): assert, documented in the header.
NNH_ASSERT(handle.valid(), "texture used after destroy");   // arrives in M1
```

Public functions document: does it allocate? is it per-frame safe? what happens on misuse?

## Headers

- A public header includes only what its declarations need (`<cstdint>`, other `nnh/` headers). No
  platform headers (`<3ds.h>`, `<SDL3/SDL.h>`) in anything under `engine/core/include/`.
- Hide implementation: opaque handles, internal headers in `engine/core/src/`, or PIMPL. Changing an
  implementation must not recompile games.
- `#pragma once`. Include order: own header, standard library, third party, project (clang-format regroups).

## Types and style

- `std::uint32_t` & co. for sizes that matter (hardware, files); `int` for ordinary counts and pixels.
- `enum class` for named values; bit-flag enums (like `nnh::Button`) document that values are bits.
- `constexpr` for constants and pure helpers (`nnh::rgb`, `screen_width`).
- Aggregates with default member initializers + designated initializers for configs:
  `nnh::init({.title = "My Game"})`.
- `const` by default; references for non-null parameters; no owning raw pointers (RAII handles).
- No `using namespace` in headers. Anonymous namespaces for file-local helpers.
- Formatting: `.clang-format` (LLVM base, 4 spaces, 110 columns). Naming: see
  [project-structure.md](../architecture/project-structure.md#naming).

## Memory on 3DS

- Know who owns every buffer and which memory it lives in (heap, linear, VRAM — see
  [devkitpro-3ds.md](devkitpro-3ds.md)).
- Allocate at load time, reuse during play. Containers like `std::vector` are fine at load time,
  not in per-frame code.

## Compile-time hygiene (build speed)

- Templates only where they pay off (e.g. typed `subscribe<T>`); keep their bodies small and in headers
  that few files include.
- Prefer forward declarations in headers; move heavy includes to `.cpp`.
- `nnh/nnh.hpp` (include-everything) is for quick experiments only, not for engine or example code.

## Further reading

- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
- Butano's design ([GValiente/butano](https://github.com/GValiente/butano)): C++ without heap/exceptions on GBA
- Mike Acton, *Data-Oriented Design and C++* (CppCon 2014)
