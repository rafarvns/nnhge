---
name: nnh-cpp
description: The C++20 subset and coding rules for NNHGE engine and tool code — no exceptions/RTTI, no per-frame heap, error handling, header hygiene, naming and formatting, 3DS memory awareness. Use whenever writing or reviewing C++ in this repository.
---

# C++ rules

Full notes: `docs/tech/cpp.md`. Naming/structure: `docs/architecture/project-structure.md`.

## Never in engine code

| Don't | Do |
|-------|----|
| Exceptions (`throw`, `try`) | Return `bool` / `std::optional` / small `Result`; `NNH_ASSERT` for programmer errors |
| RTTI (`typeid`, `dynamic_cast`) | Compile-time type ids, explicit tags |
| Heap allocation in per-frame code | Fixed-capacity arrays, pools, buffers sized at init |
| `virtual` per object in hot loops | Arrays of plain structs + one function (virtual per scene/system is OK) |
| `std::function` in hot paths | Function pointer + context, fixed-size callable |
| `<iostream>`, `<regex>`, `<locale>` | `printf`-style, hand-written parsing |
| Platform headers in `engine/core/include/` | Keep `<3ds.h>`, `<SDL3/SDL.h>` inside `engine/platforms/<target>/` |
| `using namespace` in headers | Qualify names |

## Do

- C++20: `constexpr`, `enum class`, designated initializers for configs (`nnh::init({.title = "X"})`),
  `std::uint32_t` where size matters, `const` by default, references for non-null params, RAII for ownership.
- Public functions document: allocates? per-frame safe? behaviour on misuse? (Doxygen `///`).
- Light headers: forward declarations, heavy includes in `.cpp`, opaque handles or internal headers.
- File-local helpers in an anonymous namespace. `#pragma once`.
- Know which memory each buffer uses on 3DS (heap / linear / VRAM).

## Naming and format

Types `PascalCase`; functions/variables `snake_case`; constants `kPascalCase`; macros `NNH_UPPER`;
class members with invariants end in `_`; files `snake_case`, backend files `<module>_<target>.cpp`.
Namespaces: public `nnh::`, internals `nnh::detail`, backends `nnh::platform` (`::pc`, `::ctr`).
Format with `.clang-format` (LLVM base, 4 spaces, 110 columns). Code and comments in English.

## Checklist

- [ ] Compiles with `-Wall -Wextra` and no new warnings on PC and 3DS
- [ ] No forbidden features above
- [ ] Each file one concept; functions short; names follow the conventions
