# fs — files and saves

Header: `<nnh/fs.hpp>` · Design: [architecture/security.md](../architecture/security.md)

All file access goes through `nnh::fs` and `nnh::save`, inside the **game's data folder**. Nothing
outside it can be read or written through this API (constitution X).

| System | Data folder |
|--------|-------------|
| 3DS | `sdmc:/3ds/<data_folder>/` |
| PC, Debug/Development | `<executable folder>/data/<data_folder>/` |
| PC, Release | the user's app-data folder (`SDL_GetPrefPath("NNHGE", <data_folder>)`) |

`<data_folder>` comes from `AppConfig::data_folder`, or from the title: `"Hello NNHGE"` → `hello-nnhge`.
`nnh::fs::data_root()` returns the absolute location.

## Paths

Relative, `/`-separated, at most 96 characters and 4 levels, made of `[A-Za-z0-9._-]`. Rejected with
`ErrorCode::InvalidArgument` before touching the disk: absolute paths, `..` or `.` segments, empty
segments (`a//b`), device prefixes (`sdmc:`, `C:`), backslashes, spaces and other characters.
`nnh::fs::validate_path(path)` checks a path without any file access.

## Saves

```cpp
struct Progress { std::int32_t level; std::int32_t coins; };

Progress p{3, 120};
nnh::save::write("slot1.sav", &p, sizeof p, /*version*/ 1);

std::uint16_t version = 0;
if (auto r = nnh::save::read("slot1.sav", &p, sizeof p, &version); !r) {
    if (r.error().code == nnh::ErrorCode::NotFound) start_new_game();
    else show_message("Save data is damaged");
}
```

- `save::write` writes `<path>.tmp`, keeps the previous file as `<path>.bak`, then renames the temp file
  into place. A power cut at any moment leaves a valid file.
- Each save has a 16-byte header: magic `NNHS`, your format `version`, payload size, CRC-32.
- `save::read` validates header, size and checksum. If the file is missing or damaged it loads the
  backup (and logs a warning). Errors: `NotFound`, `Corrupted` (file and backup unusable), `LimitReached`
  (buffer too small), `IoError`.
- Store plain data (fixed-size structs, no pointers); bump `version` when the layout changes.

## Files

| Function | |
|----------|-|
| `read_file(path, buffer, capacity)` | whole file → `Result<size_t>`; `LimitReached` if larger than `capacity` (nothing written past it) |
| `write_file(path, data, size)` | create/replace; **not** crash-safe |
| `write_file_atomic(path, data, size)` | create/replace with temp file + backup, like saves but no header |
| `file_size(path)`, `exists(path)`, `remove(path)` | |

Parent folders are created as needed. File functions need `nnh::init()` (else `Unsupported`).

## Files the engine writes

| File | When |
|------|------|
| `log.txt` | every log line of this run (Release: warnings and errors); the previous run is kept as `log.prev.txt` |
| `crash.txt` | full crash report (message, location, registers, last 64 log lines); the previous one is `crash.prev.txt` |

## Limits

Files are read and written whole, through fixed buffers you provide; no streaming yet. Saves up to 16 MB.
