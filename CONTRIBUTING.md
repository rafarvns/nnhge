# Contributing

Thanks for helping! The short version:

1. Read the [constitution](docs/process/constitution.md) — the rules every change is checked against.
2. Pick an item from [docs/ROADMAP.md](docs/ROADMAP.md), or open an issue for a new idea.
3. **Features start with a spec** in `specs/NNN-slug/` ([how](docs/process/sdd.md)); code starts once
   the spec is approved. Bug fixes start with a failing test.
4. **Test first** ([TDD](docs/process/tdd.md)). A change is done when it works on the PC simulator
   **and** on 3DS, has docs (header comments + `docs/api/`) and an example or test.
5. Follow the [architecture](docs/architecture/) (events + scenes) and the
   [file structure rules](docs/architecture/project-structure.md): one concept per file, small files.
6. Don't slow the build down: light public headers, no heavy dependencies ([budgets](docs/tech/cmake.md#build-speed)).
7. Format with `.clang-format`. Code and docs in English.
8. Never submit Nintendo SDK code, leaked material or copyrighted assets.

Using Claude Code? The `nnh-*` skills and agents in `.claude/` walk you through each step
([mapping](docs/process/README.md#claude-code-skills-and-agents)).

By contributing, you agree your work is released under the [zlib license](LICENSE).
