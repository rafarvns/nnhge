# Documentation

Docs are part of every change, not a follow-up. A change is not done until the docs that describe it
are true again.

## One source of truth

The Markdown files **in the repository** are the source: `docs/`, `specs/`, `README.md`,
`CONTRIBUTING.md`, plus the Doxygen comments in `engine/include/nnh/`. The
[GitHub wiki](https://github.com/rafarvns/nnhge/wiki) is **generated** from them and never edited by
hand (manual edits are overwritten).

```
repo docs ──(push to main)──> .github/workflows/wiki.yml ──> scripts/docs_tool.py wiki ──> GitHub wiki
```

## What to update when something changes

| You changed... | Update |
|----------------|--------|
| A public header `engine/include/nnh/<m>.hpp` | Doxygen comments in the header, `docs/api/<m>.md`, the example that shows it |
| A new module | New `docs/api/<m>.md`, link it from `docs/api/` users (README "Documentation" if central) |
| Keyboard mapping / simulator behaviour | `docs/api/input.md`, `docs/api/simulator.md`, `docs/tech/sdl3.md` |
| 3DS backend behaviour or limits | `docs/tech/devkitpro-3ds.md` |
| CMake, presets, scripts, CI | `docs/tech/cmake.md`; `docs/guides/getting-started.md` if setup changed; `CLAUDE.md` "Build" if commands changed |
| C++ rules | `docs/tech/cpp.md` and the `nnh-cpp` skill |
| An architecture decision | `docs/architecture/decisions.md` (row + change log) and the affected architecture page |
| Folder layout | `docs/architecture/project-structure.md`, `CLAUDE.md` "Repository layout" |
| A process rule | the `docs/process/` page, `constitution.md` if non-negotiable, and the matching skill |
| A spec's status or a finished feature | `specs/README.md`, the spec files, `docs/ROADMAP.md`, README status line |
| Build-time baseline | `docs/ROADMAP.md` table |

Skills (`.claude/skills/`) and agents (`.claude/agents/`) repeat key rules: when a rule changes, update
them too.

## Style

- English, short sentences, present tense. Tables for lookups, code blocks that compile.
- Every page starts with one `# ` heading (used as its wiki title) and ends with "Further reading" when
  it builds on outside sources.
- Link with relative paths between repository files (`../tech/cmake.md`); the wiki generator rewrites
  them to wiki pages, and other repository files to GitHub URLs.
- Don't duplicate: one page owns a topic, others link to it.
- Mark the status of designs that aren't implemented yet (`> Status: design`).

## Automatic reminder (Claude Code hook)

`.claude/settings.json` registers a **Stop hook** (`scripts/hooks/docs_reminder.py`) for everyone who
opens the repo in Claude Code. When Claude finishes a turn, it looks at the uncommitted changes:

- broken relative links → Claude is sent back to fix them;
- files changed but no Markdown/doc changed → Claude is sent back once to run the `nnh-docs` skill
  (or to say no doc is affected).

It reminds only once per set of changed files (state in `.git/nnh-docs-hook`) and never blocks twice in
a row, so it can't loop. Disable it locally with `"disableAllHooks": true` in
`.claude/settings.local.json`, or review it with `/hooks`.

## Tools

```sh
python3 scripts/docs_tool.py check                    # broken relative links (also runs in CI)
python3 scripts/docs_tool.py wiki <wiki-checkout>     # regenerate wiki pages locally (preview)
```

On Windows use `python -I scripts/docs_tool.py ...`. Preview into a scratch folder:
`mkdir -p build/wiki-preview && python -I scripts/docs_tool.py wiki build/wiki-preview`.

Wiki page names come from paths: `docs/architecture/events.md` → `Architecture-Events`,
`docs/api/input.md` → `API-Input`, `specs/001-x/spec.md` → `Spec-001-X`, `README.md` → `Home`.
The sidebar groups pages by section and uses each page's `# ` heading as the label.
Publishing happens automatically on push to `main` (`.github/workflows/wiki.yml`, also runnable by hand
from the Actions tab).
