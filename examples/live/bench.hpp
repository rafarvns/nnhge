#pragma once
// The example's bench (spec 030 AC-7, X on the showcase or LIVE_DEMO=bench): times the CPU side of live values inside
// one update — 64 show() calls per frame, registering and removing 100 tunables and 50 watches, the application point
// with edits — and logs `live bench: <phase> avg X ms worst Y ms (target < Z ms) PASS|FAIL`, then the verdict.
// Debug builds log `not judged (Debug)`.

void run_live_bench();
