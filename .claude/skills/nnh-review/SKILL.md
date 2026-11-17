---
name: nnh-review
description: NNHGE review checklist. Use to review a diff, branch or set of files against the project rules (SOLID, event architecture, file structure, C++ subset, build speed, tests, docs), or to "analyze" a spec/plan/tasks set for gaps and constitution conflicts. Reports findings; does not edit.
---

# Review checklist

Rules live in `docs/process/constitution.md`, `docs/architecture/`, `docs/process/solid.md`,
`docs/tech/cpp.md`. Report each finding with `file:line`, the rule broken, and a concrete fix.
Order findings by severity: **blocker** (breaks a constitution article or a build/test), **should fix**,
**nit**.

## Code review

**Correctness and tests**
- [ ] New behaviour has unit tests (written first); bug fixes have a regression test
- [ ] PC and 3DS both build; examples still run (screenshot) — run them if you can
- [ ] Re-entrancy/lifetime: subscriptions RAII, no dangling pointers to scenes

**Architecture (events + scenes)**
- [ ] Facts as past-tense events, requests imperative, no events used as queries
- [ ] No module reaches into another's internals; no pointers between scenes
- [ ] Dependencies point down (game → API → module → platform)

**SOLID**
- [ ] S: each file/class has one reason to change ("and" in its description → split)
- [ ] O: new behaviour added by subscribing/new scene, not by editing central switches
- [ ] L: Scene/backends honour their documented contract
- [ ] I: includes only what's used; no `nnh/nnh.hpp` in engine/examples
- [ ] D: core depends on `platform.hpp`, never on SDL/libctru directly

**C++ subset and cost**
- [ ] No exceptions, RTTI, per-frame heap, `std::function` in hot paths, `virtual` per object
- [ ] No platform headers or `#ifdef __3DS__` in `engine/include/` or game code
- [ ] 3DS memory/texture limits respected

**Safety and diagnostics (constitution X)**
- [ ] File access only through `nnh::fs` (confined to the game folder); saves atomic with checksum
- [ ] Parsers of external data bounded, with fuzz target and bad-input tests
- [ ] Bugs caught with `NNH_ASSERT`/`VERIFY`, expected failures returned as `Result`; logs via `NNH_LOG_*`
- [ ] No system SDK header or `#ifdef` system in core; system code only in its platform library
- [ ] CI changes keep least-privilege `permissions` and SHA-pinned actions

**Structure**
- [ ] One concept per file; `.cpp` ≤ 400 (aim 200), headers aim ≤ 150, functions aim ≤ 40
- [ ] Paths and names follow `docs/architecture/project-structure.md`; CMake lists explicit

**Build speed and docs**
- [ ] Public headers stayed light; no new heavy dependency (or measured and justified)
- [ ] Public API changes: header comments + `docs/api/` page + example updated
- [ ] Every doc the change made false is updated (table in `docs/process/documentation.md`);
      `python -I scripts/docs_tool.py check` reports 0 broken links; nothing edited in the wiki by hand

## Analyze (spec / plan / tasks)

- [ ] Every FR has an AC and at least one task; every task cites an FR
- [ ] Plan's Constitution Check filled before and after design; exceptions justified
- [ ] Contradictions between spec, plan and tasks
- [ ] Missing platform coverage (pc, 3ds, fake) or test strategy gaps
