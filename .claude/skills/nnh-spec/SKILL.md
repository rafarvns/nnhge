---
name: nnh-spec
description: Spec-Driven Development for NNHGE. Use when starting any new feature, writing or updating specs/NNN-slug/spec.md, plan.md or tasks.md, clarifying open questions in a spec, or checking a spec's status. Bug fixes don't need a spec, only a failing test.
---

# Spec-Driven Development

Process: `docs/process/sdd.md`. Templates: `specs/_templates/`. Index: `specs/README.md`.
Constitution (checked in every plan): `docs/process/constitution.md`.

## Workflow

1. **spec** — what and why. 2. **clarify** (optional) — answer open questions, write answers back.
3. **plan** — how, with Constitution Check before and after design. 4. **tasks** — ordered checklist.
5. **analyze** (optional, read-only) — cross-check. Then TDD per task (`nnh-tdd` skill).

**No code before the maintainer approves the spec** (status `approved`).

## Creating a spec

1. Next number = highest in `specs/` + 1 (never reuse). Folder `specs/NNN-kebab-slug/`.
2. Copy `specs/_templates/spec.md`; fill:
   - Problem and goal (2–5 sentences, the game developer's view)
   - User stories ("As a game developer, I want … so that …")
   - **FR-n** functional requirements — each testable
   - **AC-n** acceptance criteria — observable: test names, example output, screenshot, runs on 3DS
   - Non-goals, Constraints (cite constitution articles), Open questions
3. No implementation details (classes, files) unless the public API *is* the product.
4. Add a row to `specs/README.md` with status `draft`.

## Plan and tasks

- `plan.md` from template: Constitution Check table (✅ or justified exception), public API sketch, events
  introduced (table), files (new/changed + responsibility, following project-structure), data and memory
  budget, per-backend work (pc, 3ds, fake), test strategy (FR → test), risks, Constitution Check again.
- `tasks.md`: small tasks, each `files — test — covers FR-n`, `[P]` when parallelizable; always end with
  docs/api page, example, ROADMAP checkbox, status → implemented.

## Status lifecycle

`draft` → `approved` → `in progress` → `implemented` (or `superseded by NNN`). Keep the status line in all
three files and the index in sync.

## Quality checklist

- [ ] Every FR is testable and has at least one AC
- [ ] Every FR maps to a task; every task maps to an FR
- [ ] Constitution articles that apply are named
- [ ] Open questions are listed, not silently decided
- [ ] PC and 3DS behaviour both covered
