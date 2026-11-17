---
name: nnh-docs
description: Keep NNHGE documentation true after every change and publish the GitHub wiki. Use after ANY change to code, build, CI, architecture, process, skills or specs (before calling the work done), when asked to write/update docs, or to update/publish/preview the GitHub wiki. Maps what changed to the docs that must change, checks links and regenerates the wiki from docs/.
---

# Documentation and wiki

Full guide: `docs/process/documentation.md`. Constitution VIII: a change is not done until its docs are
true again.

## 1. Find what changed

`git status --short` and `git diff --stat` (plus your own memory of the task).

## 2. Update the docs it affects

| Changed | Update |
|---------|--------|
| `engine/include/nnh/<m>.hpp` | header Doxygen comments, `docs/api/<m>.md`, its example |
| new module | new `docs/api/<m>.md` |
| simulator / keyboard map | `docs/api/input.md`, `docs/api/simulator.md`, `docs/tech/sdl3.md` |
| `engine/src/platform/3ds/` behaviour, limits | `docs/tech/devkitpro-3ds.md` |
| CMake, presets, `scripts/`, CI | `docs/tech/cmake.md`; `docs/guides/getting-started.md` if setup changed; `CLAUDE.md` Build section if commands changed |
| C++ rules | `docs/tech/cpp.md` + `nnh-cpp` skill |
| architecture decision | `docs/architecture/decisions.md` (row + change log) + affected page |
| folder layout | `docs/architecture/project-structure.md`, `CLAUDE.md` layout |
| process rule | `docs/process/*.md`, `constitution.md` if non-negotiable, matching skill/agent |
| spec status / finished feature | `specs/README.md`, spec files, `docs/ROADMAP.md`, README status |
| build timings | `docs/ROADMAP.md` baseline table |

Rules: repository Markdown is the source of truth, **never edit the wiki by hand**. English, one `# `
heading per page, relative links between repo files, one page owns each topic (link, don't duplicate),
mark unimplemented designs `> Status: design`. Code in docs must compile (prefer pointing to examples).

## 3. Check

```sh
python -I scripts/docs_tool.py check      # Windows; python3 on Linux/macOS/CI
```

Must report 0 broken links. Re-read the pages you touched against the code once.

## 4. Wiki

The wiki (https://github.com/rafarvns/nnhge/wiki) is regenerated from the repo docs by
`.github/workflows/wiki.yml` on every push to `main` that touches `docs/`, `specs/`, `README.md`,
`CONTRIBUTING.md` or the tool. So: commit + push the doc changes (when the user allows pushing) and the
wiki follows. To check it published: `gh run list --workflow wiki.yml -L 1`.

- **Preview locally**: `mkdir -p build/wiki-preview && python -I scripts/docs_tool.py wiki build/wiki-preview`,
  then read the generated pages and `_Sidebar.md`.
- **Publish by hand** (only if the workflow can't, and with the user's OK — it's public):
  `git clone https://github.com/rafarvns/nnhge.wiki.git build/wiki && python -I scripts/docs_tool.py wiki build/wiki`,
  then commit and push inside `build/wiki`.
- New top-level doc section? Add it to `SECTIONS` in `scripts/docs_tool.py` so it gets wiki pages and a
  sidebar group. Page names: `docs/<section>/<file>.md` → `<Prefix>-<File>`, `README.md` → `Home`.

## Done when

- [ ] Every row of the table that applies is handled
- [ ] `docs_tool.py check` → 0 broken links
- [ ] Skills/agents updated if a rule they repeat changed
- [ ] After push: wiki workflow green
