# Plan 011: NNH Link — build and run from the console

Status: in progress (approved 2026-11-18)
Spec: [spec.md](spec.md) · Decision D24 (below) · Builds on D7, D8, D23

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ✅ | The protocol library is the same code on PC and 3DS; NNH Link's 3DS-only parts (network start, launch) go through extension headers `<nnh/3ds/...>`, selected per platform by CMake in the app — no `#ifdef` in app code |
| II 3DS constraint | ✅ | The received game is written to the SD card as it arrives (16 KB chunks); Link's own memory is small |
| III Build speed | ✅ | The protocol library is small; `nnh serve` reuses the incremental builds of spec 010 |
| IV Event-driven | ✅ | Link is scenes (server list → pairing → games → build) driven by input and session state |
| V No hidden costs | ✅ | Link's per-frame code allocates nothing; network I/O happens in its own steps with timeouts |
| VI Spec + test first | ✅ | Crypto, messages, pairing/auth state machines, server and client sessions are pure or loopback-tested first |
| VII Small files | ✅ | One concept per file in `link/` (library), `tools/nnh/src/serve/`, `apps/nnh-link/` |
| VIII Documented API | ✅ | Guide `docs/guides/nnh-link.md`; the extension headers documented in `docs/api/` |
| IX Clean and legal | ✅ | Own SHA-256/HMAC (FIPS 180-4, RFC 2104) checked with published vectors; `hb:ldr` facts from the Homebrew Launcher's sources |
| X Safe by default | ✅ | LAN only; mutual authentication before any request or program; per-message MACs; secrets never logged; wrong codes rate-limited |

## Design

### Decision D24 — one link library, mutual authentication, no encryption

`link/` holds a static library, `nnh_link`, built for PC **and** 3DS: sockets (moved from
`tools/nnh/src/net.*`; on the 3DS libctru's BSD sockets), SHA-256 and HMAC, the message codec, the
pairing and authentication state machines, and the client and server sessions. `nnh serve` and NNH Link
link the same code. Peers authenticate each other with a shared secret and every message carries a MAC;
nothing is encrypted (local network, spec non-goal). Known limit: someone recording the traffic *while
pairing* could brute-force the 6-digit code offline and learn the secret — the guide says to pair on a
network you trust, and pairing can be redone at any time.

### Protocol (TCP and UDP port 17492; the game's log stays on 17491)

**Discovery** (UDP): the console broadcasts `NNHLINK?1`; each `nnh serve` answers to the sender's
address and port `NNHSERVE1 <server id hex> <name>` (name = host name, at most 32 characters).

**Messages** (TCP): `u32 length | u8 type | payload`, little-endian, length ≤ 64 KB; after
authentication `| u32 sequence | payload | MAC-16` where MAC = first 16 bytes of
HMAC-SHA-256(session key, sequence ‖ type ‖ payload), sequences strictly increasing per direction.

| Step | Console → server | Server → console |
|------|------------------|------------------|
| hello | `Hello{version 1, console id (16 bytes), console nonce (16)}` | `Hello{server id, server nonce}` |
| pair (first time) | `Pair{HMAC(code, "pair" ‖ nonces ‖ ids)}` | `NeedPairing` right after its Hello when it doesn't know the console (added in stage 2: without it, a console that still remembers a server that forgot it would wait for a proof that never comes); then `Paired` or `Refused{reason}` (5 wrong codes → refused until restart) |
| auth (every connection) | `Proof{HMAC(secret, "console" ‖ nonces)}` | `Proof{HMAC(secret, "server" ‖ nonces)}` — sent **first**: the console checks the server before trusting anything |
| list | `ListGames` | `Games{name, title}[]` |
| build | `Build{game, release}` | `BuildLine{text}`… then `BuildFailed{last 20 lines}` or `BuildDone{size, sha256}` |
| file | — | `FileChunk{≤ 16 KB}`… |
| launch | `Launching{game}` | (shows the game's log from TCP 17491, as `nnh run`) |

Secret = HMAC-SHA-256(code, "secret" ‖ nonces ‖ ids) when paired; session key =
HMAC-SHA-256(secret, "session" ‖ nonces). Each side stores the secret by the peer's id: the console in
NNH Link's data folder (`nnh::save`, atomic), the PC in a per-user file next to `nnh.ini` (`nnh-pairs.ini`,
owner-only permissions where the OS has them).

### `nnh serve` (`tools/nnh/src/serve/`)

`nnh serve [--release]`: generates or loads its server id; prints `Pairing code: 482913 (2 min)` (a new
one on demand when a console asks to pair, shown in the terminal); answers discovery; accepts one console
at a time; builds through `build_target()` (spec 010) capturing the output to stream `BuildLine`s; sends
the file; keeps a `LogListener` on 17491 for the game started by the console; Ctrl+C stops.

### The engine's 3DS extensions (`engine/platforms/3ds/include/nnh/3ds/`)

- `<nnh/3ds/network.hpp>`: `bool network_start()` / `void network_stop()` — `socInit` with its own buffer,
  shared with the Development log redirect (reference-counted).
- `<nnh/3ds/launch.hpp>`: `bool launch_after_exit(const char* path, args...)` — `hb:ldr`
  `SetTarget`/`SetArgv` (the Homebrew Launcher's mechanism: Luma3DS Rosalina); the app then returns from
  `main`. `bool can_launch()` reports whether `hb:ldr` exists.
- Return to Link: the 3DS platform, at `nnh::shutdown()`, if the game's arguments contain
  `--nnh-return=<path>`, sets `hb:ldr`'s target to that path (games started otherwise are unaffected).

### NNH Link (`apps/nnh-link/`, built with `nnh_add_game` in every preset)

| File | Responsibility |
|------|----------------|
| `main.cpp` | init, scene stack, exit (and launch) |
| `link_state.{hpp,cpp}` | the session logic: servers, pairing, games, build progress — pure over a `Connection` interface |
| `scenes/{servers,pair,games,build}.cpp` | UI: lists with D-Pad/touch, a number pad for the code, progress, the last error lines |
| `storage.{hpp,cpp}` | console id, paired servers, last server/game (`nnh::save`) |
| `platform_link.hpp` + `platform_link_3ds.cpp` / `platform_link_pc.cpp` | network start, launch (3DS via the extension headers; PC: no launch) |

The game is saved to `sdmc:/3ds/nnh/<game>.3dsx` while it arrives (checked against the SHA-256 at the end),
then launched with `argv = {path, "<pc-ip-hex>_3DSLINK_", "--nnh-return=sdmc:/3ds/nnh-link.3dsx"}` — the
first extra argument makes the engine send its log to `nnh serve`, exactly as with the netloader.

## Test strategy

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | serve session tests against a fake console client (loopback, fake builder); `nnh serve` by hand |
| FR-2 | `link_state` tests with a fake connection (lists, build progress, errors, cancel); simulator screenshots |
| FR-3 | SHA-256 (FIPS 180-4 vectors), HMAC (RFC 4231 vectors), pairing/auth state machines: wrong code, rate limit, impostor server, unpaired console, replayed/modified message (MAC, sequence) |
| FR-4 | 3DS by hand (AC-1); argv building unit-tested |
| FR-5 | session tests: server gone mid-file, build failure, timeouts |
| FR-6 | Link builds and runs in the simulator (screenshots of each scene) and on 3DS |
| FR-7 | fuzz targets for the message decoder and discovery parser; Linux + ASan/UBSan |

## Risks and alternatives

- **`hb:ldr` details** (argv format, behaviour when Link exits) come from the Homebrew Launcher's code and
  need the console to confirm; fallback: tell the user to start the game from the Homebrew Launcher.
- **Returning to Link** depends on the game exiting normally (a crash goes to the crash screen, then the
  Homebrew Launcher).
- **Sockets on the 3DS**: libctru's BSD layer lacks some calls (no `getifaddrs`): the console only
  broadcasts to 255.255.255.255 and its own subnet (from `gethostid()`).
- **Pairing sniffing** (D24 limit) — acceptable for a LAN dev tool, documented.

## Constitution Check (after design)

No change.
