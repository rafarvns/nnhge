# Golden structured saves

Files of [spec 027](../../../specs/027-save-versions/spec.md)'s reference save (AC-2), written by the three versions
of `examples/saves`' `Progress` ([progress.hpp](../../../examples/saves/progress.hpp),
[progress_history.hpp](../../../examples/saves/progress_history.hpp)) from the values of
[reference.cpp](../../../examples/saves/reference.cpp):

| File | Written by | Bytes | Payload CRC-32 | Loaded by the v3 code: `progress_sum` |
|------|------------|-------|----------------|----------------------------------------|
| `v1.sav` | `ProgressV1` (version 1) | 1785 | `692F89F1` | `0F3BA8C8`, 2 steps |
| `v2.sav` | `ProgressV2` (version 2) | 3606 | `BB78EEE1` | `1EFDB382`, 1 step |
| `v3.sav` | `Progress` (version 3) | 3784 | `11363B5C` | `D15774A7`, no step |

Each `.sav` is a whole file (spec 002's 16-byte `NNHS` header, then the structured payload); each `.txt` is its dump
with the v3 schema (retired and unknown ids by number). `tests/saves/test_golden.cpp` checks them on every compiler;
`examples/saves/assets/golden/` holds byte-identical copies (ctest `saves_golden_copies_match`), which the example's
check mode loads on the console.

**Regenerate only with a reason written in the commit message** (a deliberate change of the format or of the
reference values), then update the sums in `test_golden.cpp`, the check mode's `check_golden.hpp` and this table:

```sh
cmake --build --preset pc-debug --target make_saves_golden
build/pc-debug/tests/make_saves_golden tests/data/saves
cp tests/data/saves/v?.sav examples/saves/assets/golden/
```
