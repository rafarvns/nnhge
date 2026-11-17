# memory — tagged allocations, stats, budgets

Header: `<nnh/memory.hpp>` · Design: [architecture/foundation.md](../architecture/foundation.md#memory)

```cpp
#include <nnh/memory.hpp>

NNH_MEM_TAG(level);                                   // once, in one .cpp

void* tiles = nnh::mem::alloc(nnh::MemRegion::Heap, NNH_MEM_TAG_REF(level), 64 * 1024);
void* verts = nnh::mem::alloc(nnh::MemRegion::Linear, NNH_MEM_TAG_REF(level), 16 * 1024, 128);
...
nnh::mem::free(verts);
nnh::mem::free(tiles);
```

## Regions

| `MemRegion` | 3DS | PC simulator |
|-------------|-----|--------------|
| `Heap` | `malloc` heap (`envGetHeapSize()`) | normal heap, not limited |
| `Linear` | GPU-visible heap for textures, vertex and audio buffers (`linearMemAlign`) | emulated, limited to 32 MB (Old 3DS) |
| `Vram` | 6 MB video memory (`vramMemAlign`); **the CPU can't write it** | emulated, limited to 6 MB |

Because Linear and Vram are limited on PC like on an Old 3DS, a game that wouldn't fit on the console runs
out of memory in the simulator too.

## Functions

| Function | |
|----------|-|
| `alloc(region, tag, size, align = 8)` | aligned block (alignment: power of two ≤ 4096); `nullptr` + warning when the region is full |
| `free(p)` | frees; `nullptr` ignored; a pointer not from `alloc` (or freed twice) is a bug (`NNH_VERIFY`) |
| `region_stats(region)` / `tag_stats(tag)` | `MemStats{bytes, peak, count}` |
| `region_capacity(region)` | bytes the system offers (0 = unknown) |
| `set_budget(region, bytes)` | warn once when usage passes `bytes` (default: the capacity); 0 disables |
| `live_allocations()`, `report_leaks()` | leaks; reported automatically at `nnh::shutdown()` in Debug/Development |

Tags: `NNH_MEM_TAG(name)` defines one, `NNH_MEM_TAG_EXTERN(name)` declares it, `NNH_MEM_TAG_REF(name)`
passes it. Up to 31 tags are counted separately; more share an "other" slot.

## Costs and rules

- Heap/Linear blocks carry a 16-byte header plus alignment padding; Vram blocks are tracked in a table
  (max 512 live), never touched by the CPU.
- Allocate at load time; don't allocate in per-frame code (constitution V).
- Memory allocated by libraries the engine uses (e.g. citro2d's internal buffers) isn't counted.
