# Plan NNN: <title>

Status: draft
Spec: [spec.md](spec.md)

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ✅ | |
| II 3DS constraint | ✅ | |
| III Build speed | ✅ | |
| IV Event-driven | ✅ | |
| V No hidden costs | ✅ | |
| VI Spec + test first | ✅ | |
| VII Small files | ✅ | |
| VIII Documented API | ✅ | |
| IX Clean and legal | ✅ | |

## Design

### Public API

```cpp
// <nnh/<module>.hpp> sketch
```

### Events introduced

| Event | Published by | Payload | Typical listeners |
|-------|--------------|---------|-------------------|

### Files

| File | New/changed | Responsibility |
|------|-------------|----------------|

### Data and memory

<Data structures, fixed capacities, memory budget on 3DS.>

### Platform work

- pc: ...
- 3ds: ...
- fake (tests): ...

## Test strategy

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | unit test `tests/<module>/test_<x>.cpp` |

## Risks and alternatives

- ...

## Constitution Check (after design)

<Re-check; explain anything that changed.>
