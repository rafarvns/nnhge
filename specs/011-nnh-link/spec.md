# Spec 011: NNH Link — build and run from the console

Status: approved (2026-11-18)
Author: NNHGE maintainers · Date: 2026-11-18
Depends on: [spec 010](../010-nnh-cli-run/spec.md) (the `nnh` tool, its netloader/log code, decision D23).
Second half of ROADMAP M6 (decisions D7, D8: builds always happen on the PC).

## Problem and goal

Trying a game on the console still starts at the PC: type `nnh run`, then press Y on the console. After
this spec the console drives it: **NNH Link**, an app on the 3DS, finds the PC running `nnh serve`, lists
its games, asks the PC to build one (only what changed), receives it, starts it — the game's log appears
on the PC as with `nnh run` — and comes back to NNH Link when the game exits, ready for the next try.
A pairing code entered once makes sure only your console and your PC talk to each other on the network.

## User stories

- As a game developer, I want to rebuild and restart my game from the console, so that I can iterate
  from the couch with the PC as a build server.
- As a game developer, I want the game's log on the PC as usual, and a build error shown on the console
  (the last lines), so that I know why nothing started.
- As a game developer, I want to return to NNH Link when the game exits, not to the Homebrew Launcher.
- As a player of my own LAN, I want no other device to push programs to my console or to start builds on
  my PC.
- As a contributor, I want NNH Link built with NNHGE itself, testable in the simulator.

## Functional requirements

- **FR-1 `nnh serve`.** A long-running mode of the `nnh` tool: it announces itself on the local network,
  lists the games of the repository, builds a game when a paired console asks (Development by default,
  Release on request), sends the `.3dsx`, receives and shows the game's log like `nnh run` (coloured,
  saved in `build/logs/`), and serves one console at a time until Ctrl+C.
- **FR-2 The NNH Link app.** A 3DS app (`.3dsx`, started from the Homebrew Launcher) that finds servers on
  the network (or takes an address), shows each one's name and whether it is paired, lists its games, and
  per game: build and run, with progress and a cancel (B). It remembers the last server and game.
- **FR-3 Pairing.** The first time, `nnh serve` shows a short code; the user enters it on the console. Both
  then keep a shared secret (console: in NNH Link's data folder; PC: per user) and every later connection
  is authenticated with it. A console accepts programs only from paired servers; a server accepts
  requests only from paired consoles. Either side can forget a pairing. Wrong codes are rate-limited.
- **FR-4 Launch and return.** The received game is saved under `sdmc:/3ds/nnh/` and started through Luma's
  homebrew loader (as the Homebrew Launcher does); its log goes to `nnh serve`; when it exits, the console
  returns to NNH Link.
- **FR-5 Clear failures.** Build errors (the last lines of the compiler output), a server that went away,
  a pairing that no longer matches, no SD space: a clear message on the console and in `nnh serve`;
  nothing waits forever (timeouts, B cancels).
- **FR-6 Built with the engine.** NNH Link uses NNHGE's scenes, text, input and assets, runs in the PC
  simulator (everything but launching, which needs the console) and on an Old 3DS.
- **FR-7 Testable.** The protocol messages, the pairing and authentication, `nnh serve`'s request handling
  (against a fake console on the loopback interface) and NNH Link's screen logic (against a fake server)
  are unit tested; the parsers are fuzzed.

## Acceptance criteria

- **AC-1** On the maintainer's Old 3DS XL, from the console only: open NNH Link, pair with the code `nnh
  serve` shows, pick `audio`, build and run it; the log shows in `nnh serve`; exiting the game returns to
  NNH Link; a second run needs no pairing.
- **AC-2** Tests show that an unpaired console can't start a build, and that the console refuses a program
  from an unpaired or impostor server.
- **AC-3** Unit tests for messages, pairing, authentication (published test vectors), serve's handling and
  Link's logic; fuzz targets for the message parsers; Linux and ASan/UBSan clean.
- **AC-4** Docs: new guide `docs/guides/nnh-link.md`; `nnh-tool.md` (`nnh serve`); `security.md` (the
  network threat row); ROADMAP M6.

## Non-goals

- Installing `.cia` files from NNH Link (needs the AM service), NNH Link itself as a `.cia` (later, once
  launching from a `.cia` title is understood).
- Encryption of the traffic (LAN only; authentication, not secrecy), internet use, several consoles at once.
- A project template (`nnh new`), browsing the SD card, debugging.
- Consoles without Luma3DS's homebrew loader (Rosalina), which the Homebrew Launcher also needs today.

## Constraints

- Constitution I: NNH Link is an NNHGE app; the 3DS-only launch goes through an extension header
  (`<nnh/3ds/...>`), never `#ifdef` in its code.
- Constitution II: Link's memory and the received file fit an Old 3DS; files are written to the SD card
  as they arrive, not held in RAM.
- Constitution IX: own implementations (protocol, SHA-256/HMAC), no third-party code; Luma's `hb:ldr`
  service is used as documented by the Homebrew Launcher's sources (facts only).
- Constitution X: LAN only; nothing runs a program from an unauthenticated peer; secrets never logged.

## Open questions

Answered by the maintainer (2026-11-18): the proposals, all accepted.

- [x] **Launching** — proposal: Luma's `hb:ldr` (set the target path, exit; the system loads it), the
  mechanism the Homebrew Launcher itself uses; NNH Link runs as a `.3dsx`.
- [x] **Returning to NNH Link** — proposal: NNH Link starts the game with an argument naming itself; the
  engine's 3DS platform, seeing it, sets `hb:ldr`'s target back to NNH Link when the game exits. Games
  started any other way are unaffected.
- [x] **Pairing and authentication** — proposal: `nnh serve` shows a 6-digit code valid for 2 minutes; the
  console sends it once; both derive a 256-bit secret; every connection then does a challenge–response
  with HMAC-SHA-256 (no encryption). Five wrong codes block pairing until `nnh serve` restarts.
- [x] **Protocol and ports** — proposal: our own small TCP protocol (versioned, length-prefixed messages)
  on port 17492, discovery by UDP broadcast on the same port; the game's log still uses 17491 as in
  `nnh run`.
- [x] **Where NNH Link lives** — proposal: `apps/nnh-link/`, built with `nnh_add_game()` in every preset
  (a new top-level `apps/` folder for programs that ship with the engine, unlike `examples/`).
