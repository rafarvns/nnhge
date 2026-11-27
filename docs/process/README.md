# How we work

Every feature goes through the same steps. Small features go through them quickly; the steps don't
change.

```
 1. spec      what and why, acceptance criteria          specs/NNN-slug/spec.md
 2. clarify   (optional) open questions answered          written back into spec.md
 3. plan      how: modules, files, events, API, risks    specs/NNN-slug/plan.md   ← Constitution Check
 4. tasks     ordered steps, each with its test           specs/NNN-slug/tasks.md
 5. analyze   (optional) cross-check spec/plan/tasks      report only
 6. build     per task: red → green → refactor (TDD)      engine/, tests/
 7. review    checklist: SOLID, structure, events, docs   report only
 8. docs      update every doc the change made false      docs/, specs/, CLAUDE.md → wiki (auto)
 9. finish    example, ROADMAP checkbox, spec status      examples/, docs/ROADMAP.md
```

The maintainer approves the spec (step 1) before any code is written.

Step 7 happens once the implementation is complete: one independent review and one final Linux pass
(`scripts/vm_check.sh final`) run together. Fixes are re-checked with the quicker `scripts/vm_check.sh fast`, then the
console runs happen back to back ([cmake.md](../tech/cmake.md), "build VM"). A review or Linux pass on a partial
implementation only finds what the next round changes anyway.

| Document | Content |
|----------|---------|
| [constitution.md](constitution.md) | Non-negotiable rules; every plan is checked against them |
| [sdd.md](sdd.md) | Spec-Driven Development: spec, plan and tasks files |
| [tdd.md](tdd.md) | Test-Driven Development: the loop, commands, fakes |
| [solid.md](solid.md) | SOLID principles applied to this engine |
| [documentation.md](documentation.md) | What to update when something changes; the generated GitHub wiki |

## Claude Code skills and agents

The repository ships skills (`.claude/skills/`) and agents (`.claude/agents/`) that follow these rules.
Anyone who opens the repo in Claude Code gets them.

| Step | Skill | Agent |
|------|-------|-------|
| 1–2 spec, clarify | `nnh-spec` | `nnh-spec-writer` |
| 3–4 plan, tasks | `nnh-architecture` | `nnh-architect` |
| 5 analyze, 7 review | `nnh-review` | `nnh-reviewer` |
| 6 build | `nnh-tdd`, `nnh-new-module`, `nnh-cpp` | `nnh-tdd-developer` |
| 3DS backend work | `nnh-devkitpro` | `nnh-3ds-specialist` |
| Build system, CI | `nnh-cmake` | `nnh-build-engineer` |
| PC simulator backend | `nnh-sdl3` | `nnh-tdd-developer` |
| 8 docs + wiki (after any change) | `nnh-docs` | every agent that edits files |

Example: *"Use the nnh-spec-writer agent to write a spec for sprite rendering"*, then
*"nnh-architect, plan spec 002"*, then *"nnh-tdd-developer, implement tasks 1–3 of spec 002"*.
