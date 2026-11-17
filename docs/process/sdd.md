# Spec-Driven Development (SDD)

We write down **what** we're building and **why** before deciding **how**, and decide how before
writing code. The spec is the contract: the plan must satisfy it, the tasks must cover it, the tests prove
it. This also gives Claude Code a precise target instead of a vague prompt.

Our layout follows [GitHub Spec Kit](https://github.com/github/spec-kit) conventions without needing its
CLI.

## Files

```
specs/
  README.md                 index of all specs with status
  _templates/               spec.md, plan.md, tasks.md
  001-event-bus-and-scenes/
    spec.md                 what and why
    plan.md                 how
    tasks.md                ordered work
```

Numbers are sequential and never reused. Slugs are short and kebab-case.

## spec.md — what and why

- **Problem and goal**: who needs this and what they can do afterwards.
- **User stories** from the game developer's point of view ("As a game developer, I want ... so that ...").
- **Functional requirements**, numbered `FR-1`, `FR-2`... Each is testable.
- **Acceptance criteria**, numbered `AC-1`... Concrete, observable (an example output, a test name).
- **Non-goals**: what this spec deliberately does not do.
- **Constraints** from the [constitution](constitution.md) that matter here (memory, build time...).
- **Open questions** — the *clarify* step answers them and writes the answers back.

No implementation details: no class names, no file paths, except the public API shape when the API *is*
the product (e.g. "games call `subscribe<T>(handler)`").

## plan.md — how

- **Constitution Check** (before design): each article, ✅ or the justified exception.
- Modules and files touched/created (following [project-structure](../architecture/project-structure.md)).
- Public API sketch (headers), events introduced, data structures, memory budget.
- Platform work per backend (pc, 3ds, fake).
- Test strategy: which requirements are unit-tested, which are verified by example/screenshot/3DS.
- Risks and alternatives considered (short).
- **Constitution Check** (after design): re-run; explain any change.

## tasks.md — ordered work

A checklist. Each task is small (one TDD loop or a few), names the files it touches, the test that
proves it, and the requirement it covers (`covers FR-2`). Tasks that can be done in any order are marked
`[P]`. The last tasks are always: docs/api page, example, ROADMAP checkbox, spec status → implemented.

## Status lifecycle

`draft` → `approved` (maintainer said yes) → `in progress` → `implemented`. A spec can also be
`superseded by NNN`. Status is the first line under the title of each file.

## Analyze (optional, read-only)

Before coding, cross-check the three files: requirements with no task, tasks with no requirement,
contradictions, constitution violations. The `nnh-reviewer` agent does this when asked.

## Lean path

For experiments or tiny changes: a short spec (problem + acceptance criteria), a plan of a few lines,
and tasks. Bug fixes don't need a spec; they need a failing test first.

## Further reading

- GitHub Spec Kit: [repository](https://github.com/github/spec-kit) and its
  [spec-driven.md](https://github.com/github/spec-kit/blob/main/spec-driven.md) philosophy.
