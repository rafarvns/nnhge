#!/usr/bin/env python3
"""Claude Code Stop hook: keep docs in sync with changes (docs/process/documentation.md).

When Claude finishes a turn it checks the uncommitted changes:
  - broken relative links in Markdown          -> block with the list (fix them)
  - non-doc files changed but no doc changed    -> block once with a reminder to run the nnh-docs skill
Each distinct set of changed files is reminded about only once (state in .git/nnh-docs-hook), and a
Stop that is already continuing because of this hook (stop_hook_active) is never blocked again.
"""

from __future__ import annotations

import hashlib
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
STATE = ROOT / ".git" / "nnh-docs-hook"
IGNORED_PREFIXES = ("build/", "out/", ".cache/")


def changed_files() -> list[str]:
    out = subprocess.run(["git", "status", "--porcelain", "--untracked-files=all"], cwd=ROOT,
                         capture_output=True, text=True, check=False).stdout
    files = []
    for line in out.splitlines():
        path = line[3:].split(" -> ")[-1].strip().strip('"')
        if path and not path.startswith(IGNORED_PREFIXES):
            files.append(path)
    return files


def is_doc(path: str) -> bool:
    return path.endswith(".md") or path.startswith(("docs/", "specs/"))


def broken_links() -> str:
    result = subprocess.run([sys.executable, "-I", str(ROOT / "scripts" / "docs_tool.py"), "check"],
                            cwd=ROOT, capture_output=True, text=True, check=False)
    return result.stdout.strip() if result.returncode != 0 else ""


def block(reason: str) -> None:
    print(json.dumps({"decision": "block", "reason": reason}))
    sys.exit(0)


def main() -> None:
    try:
        payload = json.load(sys.stdin)
    except (json.JSONDecodeError, ValueError):
        payload = {}
    if payload.get("stop_hook_active"):
        return

    files = changed_files()
    if not files:
        return

    links = broken_links()
    if links:
        block("Documentation has broken links (scripts/docs_tool.py check):\n" + links +
              "\nFix them (nnh-docs skill) before finishing.")

    code = [f for f in files if not is_doc(f)]
    if not code or any(is_doc(f) for f in files):
        return

    key = hashlib.sha256("\n".join(sorted(files)).encode()).hexdigest()
    if STATE.exists() and STATE.read_text(encoding="utf-8").strip() == key:
        return  # already reminded about exactly this set of changes
    STATE.write_text(key, encoding="utf-8")

    listed = "\n".join(f"  - {f}" for f in code[:15]) + ("\n  ..." if len(code) > 15 else "")
    block("Files changed but no documentation was updated:\n" + listed +
          "\nRun the nnh-docs skill: update every doc this change made false (table in "
          "docs/process/documentation.md). If no doc is affected, say so briefly and finish.")


if __name__ == "__main__":
    main()
