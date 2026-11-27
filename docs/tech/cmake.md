# CMake

One CMake project builds every target. The platform is chosen by the preset (toolchain), never by
options in game code. The same engine is also a subproject of a game's own project
([game projects](#game-projects-the-engine-as-a-subproject)).

## Presets

| Preset | Target | Toolchain |
|--------|--------|-----------|
| `pc-debug`, `pc-dev`, `pc-release` | PC simulator | host compiler (MinGW GCC, MSVC, clang) |
| `3ds-debug`, `3ds-dev`, `3ds-release` | Nintendo 3DS | `$DEVKITPRO/cmake/3DS.cmake` (run via `scripts/dkp.sh` on Windows) |

The suffix is the build flavor ([docs/api/build.md](../api/build.md)): `-debug` → `Debug`,
`-dev` → `RelWithDebInfo` (Development), `-release` → `Release`. Hidden presets `flavor-*` hold the
build types; platform presets combine `base`/`3ds-base` with one of them.

```sh
cmake --preset pc-debug && cmake --build --preset pc-debug && ctest --preset pc-debug
scripts/dkp.sh cmake --preset 3ds-release && scripts/dkp.sh cmake --build --preset 3ds-release
```

Build folders: `build/<preset>/`. Personal presets go in `CMakeUserPresets.json` (git-ignored).

## Project layout in CMake

| File | Role |
|------|------|
| `CMakeLists.txt` | Project, C++20, platform detection (`NINTENDO_3DS` → `NNH_PLATFORM`), ccache, subdirectories |
| `engine/CMakeLists.txt` | Adds core + the preset's platform library; defines `nnh::engine` (INTERFACE: core + platform), what games link |
| `engine/core/CMakeLists.txt` | the core's sources as the OBJECT library `nnh_core_objects`, and `nnh::core_contract` (contract headers, INTERFACE) |
| `engine/core/archive/CMakeLists.txt` | `nnh::core` (static, `engine/core/libnnh_core.a`) archived from `nnh_core_objects`; a response file in 3DS presets ([below](#the-command-line-limit-windows-3ds)) |
| `engine/platforms/<system>/CMakeLists.txt` | `nnh::platform_<system>` (static), links the system SDK `PRIVATE` |
| `cmake/nnh_engine_options.cmake` | `nnh_engine_options(target)`: C++20, warnings, no exceptions/RTTI |
| `cmake/nnh_game.cmake` | `nnh_add_game()` |
| `cmake/nnh_platform.cmake`, `nnh_version.cmake`, `nnh_game_defaults.cmake`, `nnh_optional_parts.cmake` | the subproject contract (spec 032, [below](#game-projects-the-engine-as-a-subproject)): `nnh_platform()`, `NNHGE_VERSION*`, game target defaults, `nnh_optional_part()` |
| `cmake/nnh_assets.cmake` | `nnh_add_assets()` (the `ASSETS` folder of `nnh_add_game`), with `nnh_asset_index.cmake` (the index and its name checks) |
| `cmake/nnh_images.cmake` | `nnh_add_images()` (the `IMAGES` of `nnh_add_game`), with the build-time scripts `nnh_check_image.cmake` and `nnh_embed.cmake` |
| `cmake/nnh_sdl3.cmake` | Provides `SDL3::SDL3` for the PC platform |
| `cmake/nnh_host_tools.cmake` | Target `nnh_assetc` in every preset, `nnh_assetc_command()` and `_nnh_host_cxx()` (host tools, below) |
| `cmake/nnh_data.cmake` | `nnh_add_data_schema()` (the `DATA_SCHEMA` of `nnh_add_game`) and `nnh_use_data_schema()` (data assets, below) |
| `tools/data_schema/CMakeLists.txt` | the data schema programs: `nnh_data_schema_support` (PC) or one game's program (3DS `ExternalProject`) |
| `examples/CMakeLists.txt` | One `add_subdirectory` per example |
| `tests/CMakeLists.txt` | `nnh_tests` (core + fake; ctests `unit` and `unit_reverse`, the same tests in reverse order to catch state one test leaves for the next), `nnh_conformance_<platform>` (PC only) |

## Common changes

**Add a source file**: core files go in `engine/core/CMakeLists.txt`; a system's files go in
`engine/platforms/<system>/CMakeLists.txt` (and every platform implements each new contract function,
fake included). Sources are always listed explicitly.

**Add a platform**: new folder `engine/platforms/<system>/` with a `nnh_platform_<system>` target and
presets selecting it ([platforms.md](../architecture/platforms.md)).

**Add an example**: `examples/<name>/CMakeLists.txt` with `nnh_add_game(...)`, then
`add_subdirectory(<name>)` in `examples/CMakeLists.txt`. Apps that ship with NNHGE (`apps/nnh-link`, spec
011) are added the same way under `apps/`, built in every preset of the engine repository (`NNH_BUILD_APPS`, independent
of `NNH_BUILD_EXAMPLES`; OFF in a game project);
they may link more libraries after `nnh_add_game()` (`target_link_libraries(nnh-link PRIVATE nnh_link)`)
and pick a platform file with `src/platform_link_${NNH_PLATFORM}.cpp`.

```cmake
nnh_add_game(sprites
    SOURCES main.cpp
    ASSETS assets                       # optional folder: assets/art/hero.png -> load_texture("art/hero")
    IMAGES art/logo.png                 # optional embedded images: load_texture("logo")
    DATA_SCHEMA data_types.cpp          # optional: typed data assets (spec 014, "Data assets" below)
    TITLE "Sprites demo" DESCRIPTION "NNHGE example" AUTHOR "NNHGE contributors"
    VERSION 1.0.0                       # optional, default 0.1.0 (major.minor.micro, at most 63.63.15)
    ICON icon.png                       # optional 48x48 PNG; the NNHGE icon otherwise
    CIA UNIQUE_ID 0xE0A01)              # optional: an installable .cia (spec 009, see below)
```

On 3DS this calls devkitPro's `ctr_generate_smdh()` and `ctr_create_3dsx()`; on Windows it copies
`SDL3.dll` next to the executable. **Package metadata** (spec 009, `cmake/nnh_package_rules.cmake`) is
checked at configure time, naming the field: title and author at most 64 characters, description 128
(counted in UTF-8 characters), `VERSION` major and minor 0–63 and micro 0–15; with `CIA`, a `UNIQUE_ID`
in the homebrew range `0x00300`–`0xF7FFF`, an optional `PRODUCT_CODE` like `CTR-H-ABCD` (default
`CTR-H-` + the target's first 4 letters), optional `BANNER` (256×128 PNG) and `BANNER_AUDIO` (16-bit
WAV, at most 3 s), with NNHGE defaults from `cmake/3ds/` (drawn by `scripts/make_package_art.py`). The
icon is checked by a build step on every system (`nnh_check_icon.cmake`), so a wrong icon fails on PC
too. `TITLE` is also compiled into the game as `NNH_GAME_TITLE`, the default `AppConfig::title`. Script
tests: `icon_check_*`, `banner_check_*`, `package_check_*`, `package_resolve_*`, `game_title_*`
(fixtures in `tests/data/package/`).

**Distribution** (3DS, spec 009, `cmake/nnh_cia.cmake`): none of these is part of the normal build. `<game>_dist` copies the `.3dsx` (and, with `CIA`, the `.cia`) into `build/<preset>/dist/<game>/`; `dist` does it for every game; `<game>_cia` builds `dist/<game>/<game>.cia` with makerom from the same ELF, SMDH and assets folder (as romfs; an empty one without `ASSETS`), our RSF `cmake/3ds/nnhge.rsf` and a banner made by bannertool. The tools are fetched by the target `nnh_ctr_tools` the first time a `.cia` is built (`cmake/nnh_fetch_ctr_tools.cmake`: makerom v0.19.0 and bannertool 1.2.3, Windows and Linux x86-64, pinned by SHA-256, into `build/<preset>/_tools/`), so configuring and building games downloads nothing. Offline or on macOS, configure with `-DNNH_MAKEROM=<path> -DNNH_BANNERTOOL=<path>`.

**Assets** (`ASSETS <folder>`, spec 007, the recommended way): every file of the folder becomes an asset
named by its relative path ([api/assets.md](../api/assets.md)). One build step per file, rerun only when
that file changes, writes into `build/<preset>/<game dir>/assets/<path>`: PNGs are checked like images
(below) and, on 3DS, converted by `tex3ds` under the same name; `.wav` and `.ogg` files are checked by
`nnh_check_sound.cmake` (header only: PCM 8/16-bit WAV or Ogg Vorbis, mono or stereo, 8000–48000 Hz —
`jump.wav is 24-bit: save it as PCM 16-bit (or 8-bit)`; ctest cases `sound_check_*`, fixtures in
`tests/data/sounds/`) and copied; Tiled maps (`.tmj`, spec 012) are converted by `nnh-assetc` (below) into
`<path>.nnhmap`, one step per map with a Ninja `DEPFILE` (in `build/<preset>/<game dir>/nnh_maps/`) listing the
tilesets and images it read, so editing a shared `.tsj` or tileset image reconverts exactly the maps that use it;
`.tsj`, `.tiled-project` and `.tiled-session` files are not packed (`nnh_asset_output_name()`; a `.tmj` and a
`.nnhmap` of the same name are a configure error); Aseprite exports (`<name>.ase.json`, spec 016, D31) are
converted by `nnh-assetc sprite` into `<name>.nnhsprite`, one step per export with a depfile (in
`build/<preset>/<game dir>/nnh_sprites/`) listing the JSON and the PNG beside it, and Aseprite's `.ase`/`.aseprite`
files are not packed (the `.ase.json` rule comes before every generic one, the data step included); other files are
copied — or, in a game with `DATA_SCHEMA`, go through the data steps ([Data assets](#data-assets-data_schema)). Then
`nnh_asset_index.cmake` writes `assets/.nnh_assets` (the sorted list of output names core reads at init) and fails the
build for names that break the asset path rules, use characters outside `A-Z a-z 0-9 space - _ .`, are
reserved, or differ from another only by letter case; it also deletes outputs of removed assets. On PC
that folder is next to the executable; on 3DS the target `<game>_assets` is a devkitPro asset target
passed as `ROMFS` to `ctr_create_3dsx`, so the `.3dsx` is repacked when an asset changes. At configure time
`nnh_add_assets()` also writes `<binary dir>/nnh_reload_<game>.txt` (`assets=`, `game=`, `out=`: the source folder, the
game's folder and the converted folder) for `nnh run`'s hot reload (spec 031), which builds `<game>_assets` alone and
dry-runs `<game>` (`-- -n`) to tell whether a change needs a rebuild. The folder is
scanned with `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` — the one exception to "no GLOB" below: assets
are data, and adding a file must not require editing CMake. Script tests: `asset_index_*` (fixtures in
`tests/data/asset_lists/`; `asset_index_names_maps_by_their_output` checks the output names).

**Images** (`IMAGES`, decision D19: embedded in the executable, for small games; `ASSETS` is the
recommended way): one build step per image, rerun only when that image changes:

1. `nnh_check_image.cmake` reads the PNG header and fails the build unless both sides are powers of two
   from 8 to 1024 — `hero.png is 100x60: textures need power-of-two sides from 8 to 1024 ...`;
2. on 3DS, `tex3ds -f rgba8888 -z auto` converts it to a `.t3x`; on PC the PNG stays as is;
3. `nnh_embed.cmake` writes `image_<name>.cpp` (a byte array).

A registry `images.cpp` maps names (file name without folder and extension; duplicates are a configure
error) to the arrays; games without `IMAGES` link an empty default from core. Generated files live in
`build/<preset>/<game dir>/nnh_images/<game>/`. The check script has its own `ctest` cases
(`image_check_*`, fixtures in `tests/data/images/`).

**Host tools** (`tools/`): `tools/nnh/CMakeLists.txt` builds the `nnh` program (spec 010; PC presets only, never a dependency of games) from `nnh_tool_lib` and its tests `nnh_tool_tests` (ctest `nnh_tool`). `nnh_tool_lib` also compiles the pure core
files `engine/core/src/remote/remote_wire.cpp` and `core/crc32.cpp` (spec 013: the console remote's wire format and
the screenshots' PNG CRCs), with `engine/core/src` on its include path, so both ends share one implementation. With MinGW they link the C++ runtime statically (`-static`): Git for Windows puts an older `libstdc++-6.dll` first on the PATH, which made the test program fail to start (exit code 127, no output) as soon as it used `std::filesystem`.

**nnh-assetc** (spec 012, decision D27): the asset converter the build runs on Tiled maps and Aseprite exports
(spec 016: `nnh-assetc sprite`; version 0.4.0 since spec 019 checks `collision_layers`), so games with maps or exports need it in **every** preset, including cross ones that have no host compiler of their own.
`cmake/nnh_host_tools.cmake` declares the target `nnh_assetc` everywhere, and `nnh_assetc_command(<cmd> <deps>)`
gives an asset step the program to run and what to depend on:

| Preset | `nnh_assetc` is | Built |
|--------|-----------------|-------|
| PC | the executable of `tools/assetc/` (`build/<preset>/tools/assetc/nnh-assetc`), ctest `assetc_*` (`assetc_unit` = `nnh_assetc_tests`) and `map_convert_*` | with everything |
| 3DS | an `ExternalProject` configuring `tools/assetc/` as its own project with the host compiler `NNH_HOST_CXX`, in Release, into `build/<preset>/_host/assetc/` | only when a step needs it or you build the target (`EXCLUDE_FROM_ALL`); a 3DS build without maps never pays for it |
| 3DS with `-DNNH_ASSETC=<path>` | a prebuilt `nnh-assetc` (e.g. `build/pc-debug/tools/assetc/nnh-assetc.exe`), like `NNH_MAKEROM`; the target just prints its version | never |

`tools/assetc/CMakeLists.txt` is therefore standalone-capable (its own `project()`, includes
`nnh_engine_options.cmake` itself; standalone it builds only the program, no tests) and compiles pure core files
(`core/crc32.cpp`, `tilemap/map_layout.cpp`, `map_strings.cpp`, `map_property_table.cpp`, `map_validate*.cpp`;
for data assets, spec 014: `core/crc32_update.cpp`, `text/utf8_valid.cpp`, `assets/asset_path.cpp` and the pure
`data/` files — registry, schema check, fingerprint, description writer, `data_validate.cpp`, `index_validate.cpp`)
into `nnh_assetc_lib`, so the console and the converter share one implementation; it also needs
`engine/core/include` (`<nnh/data.hpp>`'s tables, `<nnh/detail/name_hash.hpp>`). It links the C++ runtime
statically when the compiler is MinGW (checked with `CMAKE_CXX_PLATFORM_ID`, because inside devkitPro's MSYS2 the
host CMake is MSYS's while the compiler is MinGW). The top-level `CMakeLists.txt` sets it up (and calls
`enable_testing()`) **before** `examples/`, so a game's asset step can ask for the program.

The command the asset step runs (one per map, from the game's build folder):

```sh
nnh-assetc map <in.tmj> <out.nnhmap> --assets <assets folder> --depfile <out.d>   # exit 1 + "file: reason" on error
nnh-assetc data <in> <out> --schema <schema.nnhschema> --refs <out.refs> --name <asset name> [--show <path>]
nnh-assetc data-index --out <.nnh_data> --schema <schema.nnhschema> --assets-list <names> --refs-list <sidecars>
                      [--show <assets folder>]
nnh-assetc --version                                                               # another command line: usage, exit 2
```

On failure it prints `file: reason` (the file the problem is in: the map, a tileset or an image) and removes any
old output. `data` and `data-index` are the data asset steps (spec 014, next section). **Depfile paths are relative to the working directory**: CMake reads them from there, and devkitPro's
MSYS CMake took the native program's absolute `E:/...` paths for relative ones (prefixing the build folder), so every
map reconverted on every build. Converter fixtures are in `tests/data/maps/` (PNGs by `make_fixtures.py`); the
`map_convert_rejects_*` / `map_convert_explains_*` ctests run it on each bad map, and `nnh_tests` loads
`good.tmj` converted at build time (`tests/CMakeLists.txt`). With `-DNNH_BUILD_FUZZERS=ON` and clang,
`fuzz_assetc_json`, `fuzz_assetc_map`, `fuzz_assetc_toml`, `fuzz_assetc_data` and `fuzz_assetc_refs` are built in
`build/<preset>/tools/assetc/`. The engine's run-time readers get theirs in `build/<preset>/tests/` (each
`tests/fuzz/libfuzzer_<name>.cpp` calling a target of `fuzz_targets.hpp` that `nnh_tests` also runs as a mini-fuzz);
spec 029 adds `fuzz_font_validate` (`.nnhfont`), `fuzz_loc_table` (`.nnhstr`) and `fuzz_loc_manifest` (`.nnh_loc`),
and `nnh-assetc`'s font and string converters `fuzz_assetc_{bmfont,ttf,png_pages,font_desc,strings}`.

`NNH_HOST_CXX` (cache variable) defaults to the environment variable of the same name, else `c++`/`g++`/`clang++`
on the PATH (Linux: the system compiler). With none, 3DS presets still configure and build; only `nnh_assetc` (and
games with maps, at configure time) fail, saying to set `NNH_HOST_CXX` or `NNH_ASSETC`. **On Windows** devkitPro's MSYS2 has no
host compiler and starts with an almost empty environment (Git Bash's variables don't reach it), so
`scripts/dkp.sh` finds `g++` (else `c++`, `clang++`) on Git Bash's PATH — or takes your `NNH_HOST_CXX` — and hands
it over as an argument; inside it is exported in MSYS form (`/c/...`, or `/home/...` for `C:/Users`), because
MSYS's CMake misreads `C:/...` as a list. Paths given with `-D` inside `dkp.sh` must use that form too
(`-DNNH_ASSETC=/e/.../nnh-assetc.exe`). `nnh` does the same since spec 032: its first configure of a 3DS preset on
Windows passes `-DNNH_HOST_CXX=/c/...` (found the same way), so `nnh build` works in a fresh 3DS build folder without
`dkp.sh` ([nnh-tool.md](../guides/nnh-tool.md#game-projects)). The first build of the tool takes about 9 s on Windows (host compiler
detection; under 1 s on Linux). Since spec 031 (T10.3) the sub-build runs only when one of the files it compiles
changed: its build step depends on `tools/assetc/**`, the engine core sources and headers and the vendored stb
libraries (globbed at configure time; a new source file comes with a change of `tools/assetc/CMakeLists.txt`, which is
in the list). Before, `BUILD_ALWAYS` ran the sub-build's three steps on every 3DS build of a game with maps or data
(≈ 1.2 s on Windows through MSYS): a no-op `data_assets` went from 2.7 s to 1.9 s through `scripts/dkp.sh`, a data-file
edit from 2.8–3.0 s to 1.6 s with `nnh`'s Ninja call. If the tool's executable is deleted by hand, touch one of its
sources (or delete `build/<preset>/_host/assetc-ep/`) to rebuild it. Numbers in the [ROADMAP](../ROADMAP.md) table.

```sh
scripts/dkp.sh cmake --build --preset 3ds-release --target nnh_assetc   # build it for the host explicitly
build/3ds-release/_host/assetc/nnh-assetc --version                       # nnh-assetc 0.4.0 (crc32 CBF43926)
```

### Data assets (DATA_SCHEMA)

`nnh_add_game(<game> ... DATA_SCHEMA <schema sources> ASSETS <folder>)` (spec 014, decision D29,
[api/data.md](../api/data.md)); `cmake/nnh_data.cmake` and `cmake/nnh_assets.cmake`:

1. **Schema sources** form an OBJECT library `<game>_data_schema_objs`, compiled like the game's sources and added
   to the game as objects (never through an archive, so the static registrations can't be dropped). Include
   directories: the engine's public headers, the folder of the calling `CMakeLists.txt` and each schema source's.
2. **The schema program** `<game>_data_schema` = those declarations + `tools/data_schema/schema_main.cpp` + core's pure
   registry files, for the host. It checks the schema (two types with one key: a build error naming both sources)
   and writes `build/<preset>/<game dir>/nnh_data/<game>.nnhschema`, only when its text changes.
   - **PC presets:** an executable linking the very same objects with `nnh_data_schema_support` (defined by
     `tools/data_schema/CMakeLists.txt`, added from `nnh_host_tools.cmake`): nothing is compiled twice.
   - **Cross presets (3DS):** an `ExternalProject` building `tools/data_schema/` as a project of its own with the host
     compiler (`NNH_HOST_CXX`, found by `_nnh_host_cxx()` in `nnh_host_tools.cmake`, as for `nnh-assetc`), into
     `nnh_data/<game>_host/`. No `BUILD_ALWAYS`: its build step depends on a stamp that depends on the console
     objects of the schema sources (`$<TARGET_OBJECTS:...>`) and on the program's own engine sources, so it reruns
     only when a schema source, a header it includes, or that engine code changes. Without a host compiler the 3DS
     build still configures; the data game's schema step fails saying to set `NNH_HOST_CXX`. A prebuilt
     `-DNNH_ASSETC` doesn't replace this program (it holds the game's own declarations).
3. **One data step per asset** the pipeline doesn't otherwise own (not images, sounds, maps, Tiled files, nor
   Aseprite's `*.ase.json`/`*.ase`/`*.aseprite`, D31): `nnh-assetc data` converts files whose extension is a
   declared key and copies the rest, under the same asset name, and writes a references sidecar in
   `nnh_data/<game>/refs/` (format: `tools/assetc/src/data_refs.hpp`). Messages show paths relative to the game's
   folder: `assets/enemies/goblin.enemy:9: ...` (at most 20 per file).
4. **The index step** `nnh-assetc data-index` reads every sidecar (listed in `nnh_data/<game>/refs.list`, relative to
   that folder: in cross presets devkitPro's MSYS CMake would write absolute `/e/...` paths the native tool can't
   open) and the asset list, checks names, references, media kinds, hash collisions and counts, and writes
   `assets/.nnh_data`, which joins the asset index (`nnh_asset_index.cmake` reserves the name unless
   `-DGENERATED=.nnh_data`; ctests `asset_index_*_data`).

Both tools and the schema program rewrite an output only when its bytes change (Ninja's `restat`), so: editing a
data file reruns its step and the asset index (+ the `.3dsx` repack), with no C++ compile or link — a comment-only
edit stops after its own step; a schema edit that doesn't change the schema (a comment, a default value) recompiles
the schema source (console + host on 3DS, once on PC) and relinks, but reconverts nothing; a schema change reconverts
every data file. Measured in [tasks.md](../../specs/014-data-assets/tasks.md) (T7.7, T7.8) and the
[ROADMAP](../ROADMAP.md) table. Tests: `data_convert_*` and `data_index_rejects_*` (fixtures in
`tests/data/data_assets/`, against its committed `schema.nnhschema`, kept equal to the compiled tests' schema by
`data_schema_program_writes_the_fixtures_schema`), `data_schema_program_refuses_a_key_used_twice`,
`data_build_{fails,explains}_with_file_line` (builds the test game `nnh_test_broken_data_assets`), and the
round trip in `nnh_tests` (`tests/data/data_assets/game/` converted by the real pipeline, target `nnh_test_data`).

**Shared link library** (`link/`, every preset, spec 011): `nnh_link` holds the network code `nnh` and the
NNH Link console app share (sockets, SHA-256/HMAC, message codec, pairing). It never depends on
`nnh::core`, so the tool doesn't pull the engine in; on the 3DS it builds against libctru's sockets.
Its tests `nnh_link_tests` (ctest `nnh_link`, also static on MinGW) include a mini-fuzz; with
`-DNNH_BUILD_FUZZERS=ON` and clang, `fuzz_link_{messages,frames,discovery}` are built in `build/<preset>/link/`.

**Add a test file**: list it in `tests/CMakeLists.txt`.

**Add a dependency**: avoid it. If unavoidable: small, permissive license, fetched with a pinned URL and
`URL_HASH SHA256=...`, linked `PRIVATE`, and the build-time impact measured (constitution III). A
single-file library may instead be vendored, unmodified, in `third_party/<name>/` with a `README.md`
(upstream, commit, SHA-256, license) and its own `CMakeLists.txt` building an object library with
warnings off — like `third_party/stb_vorbis` (spec 008), linked into `nnh_core`. `third_party/stb_truetype` and
`third_party/stb_image` (spec 029, `nothings/stb` commit `9f755b6`, PNG only) are host-only: linked into `nnh-assetc`
for the font converter, never into a game; `nnh_add_game(... LANGUAGES en pt-BR ja [BASE en] [STRICT_STRINGS])`
adds the string steps (`cmake/nnh_strings.cmake`); see [third-party.md](third-party.md).

**`nnh_tests` on MinGW links libstdc++ and libgcc statically** (`-static-libstdc++ -static-libgcc`, tests only; lead
decision during spec 029, confirmed by the maintainer 2026-11-26): Git Bash puts its own `/mingw64/bin` (a `libstdc++-6.dll`
built against `msvcrt.dll`) before the compiler's on `PATH`, and a string that DLL allocated, freed by an inlined Release
destructor through the UCRT, corrupted the heap. Games use no C++ runtime allocation and keep the default linking.

## Game projects: the engine as a subproject

Spec 032 (D55): a game project adds the engine with `FetchContent` (a hash-pinned archive, or the local override
`FETCHCONTENT_SOURCE_DIR_NNHGE`) or `add_subdirectory` (a git submodule, a copied folder), and calls `nnh_add_game()`
from any of its folders ([guide](../guides/game-projects.md)). What makes that work:

- **Caller-independent helpers.** Every helper finds the engine's files through `CMAKE_CURRENT_FUNCTION_LIST_DIR`,
  read inside the function, never through variables set when its file was included (a game's folder doesn't have
  them). The system comes from the global property `NNH_PLATFORM`, set by the engine's top-level `CMakeLists.txt` and
  read with `nnh_platform(<out> <who>)` (`cmake/nnh_platform.cmake`): an empty or unknown value fails the configure,
  `nnh_add_game(mygame): unknown platform "wii" (supported: pc, 3ds)`, before any target is made. The engine's own
  folders keep their `NNH_PLATFORM` variable.
- **Optional parts.** `NNH_BUILD_EXAMPLES` (examples), `NNH_BUILD_TESTS` (unit, conformance and fuzz tests, the
  consumer tests; PC presets), `NNH_BUILD_APPS` (NNH Link) and `NNH_BUILD_TOOLS` (the `nnh` tool and its tests; PC
  presets) default to the engine's `PROJECT_IS_TOP_LEVEL`: ON in the engine repository, OFF in a game project. Each is
  added by `nnh_optional_part(<option> <folder>)` (`cmake/nnh_optional_parts.cmake`), which fails the configure when the
  option is ON and the folder is missing (a pruned copy). `link/` is configured only with apps, tools or tests. The host
  programs (`nnh-assetc`, the data schema support) are never behind an option.
- **Game target defaults** (`cmake/nnh_game_defaults.cmake`): every target `nnh_add_game()` creates (the game,
  `<game>_data_schema_objs`, `<game>_data_schema`) gets C++20 without extensions unless the calling folder set
  `CMAKE_CXX_STANDARD` / `CMAKE_CXX_EXTENSIONS`, and `NNH_CCACHE` as its compiler launcher unless the calling folder set
  one. Inside the repository the calling folders already have all three, so nothing changes. `NNH_CCACHE` is searched
  only when undefined or `*-NOTFOUND`: `-DNNH_CCACHE=` turns the cache off.
- **Version.** `cmake/nnh_version.cmake` sets `NNHGE_VERSION`, `NNHGE_VERSION_MAJOR`, `_MINOR`, `_PATCH` (INTERNAL cache
  entries, from `project(nnhge VERSION ...)`) and prints `NNHGE <version>: <engine folder>` at every configure.
- **Outputs** go under `CMAKE_BINARY_DIR` (the game's build folder): the manifest, `dist/`, `_tools/`, `_host/`; the
  engine's own build under `_deps/nnhge-build/` (FetchContent) or the `add_subdirectory` binary folder. Nothing is
  written into the engine's source folder.

### Reserved names

Targets the engine defines in a game's build (a game must not use these names): `dist`, `nnh_assetc`,
`nnh_ctr_tools` (3DS), `nnh_engine`, `nnh_core`, `nnh_core_objects`, `nnh_core_contract`, `nnh_platform_pc` or
`nnh_platform_3ds`, `nnh_raster3d` (PC), `nnh_mesh3d_shader`, `nnh_mesh3d_shbin` (3DS), `nnh_stb_vorbis`, `nnh_ryu`,
`nnh_stb_image`, `nnh_stb_truetype`, `nnh_assetc_lib`, `nnh_data_schema_support` (PC); per game `<game>_icon`,
`<game>_assets`, `<game>_3dsx`, `<game>_dist`, `<game>_cia`, `<game>_data_schema`, `<game>_data_schema_objs`,
`<game>_data_description` and `<game>.elf`/`<game>.smdh` files on 3DS; the aliases `nnh::*`; `nnh_link` with apps,
tools or tests. On PC the simulator's SDL3 adds its own: a system SDL3 (`find_package`) adds `SDL3::SDL3` only; otherwise the
FetchContent names `nnh_sdl3_prebuilt` (MinGW and MSVC: the official development package, no compile) or `nnh_sdl3`
(Linux, macOS: built from source), each with its override `FETCHCONTENT_SOURCE_DIR_NNH_SDL3_PREBUILT` /
`FETCHCONTENT_SOURCE_DIR_NNH_SDL3`; built from source, SDL3's own targets join the build (`SDL3-static`, `SDL3_Headers`,
`SDL_uclibc` and its other internal ones), and `cmake/nnh_sdl3.cmake` sets `SDL_SHARED=OFF`, `SDL_STATIC=ON`,
`SDL_TEST_LIBRARY=OFF` and `SDL_EXAMPLES=OFF` as cache entries **with FORCE: they override the game project's own
values** of those four (a game that builds SDL3 itself must not rely on them). Cache entries: `NNH_BUILD_EXAMPLES`, `NNH_BUILD_TESTS`, `NNH_BUILD_APPS`, `NNH_BUILD_TOOLS`,
`NNH_CCACHE`, `NNH_ASSETC`, `NNH_HOST_CXX` (3DS), `NNH_MAKEROM`, `NNH_BANNERTOOL` (3DS), `NNHGE_VERSION*`, and with
the tools `NNH_RELEASE_ENGINE_URL`, `NNH_RELEASE_ENGINE_SHA256`. Global
properties: `NNH_PLATFORM`, `NNH_ASSETC_PROGRAM`, `NNH_ASSETC_MISSING`.

### The command-line limit (Windows, 3DS)

devkitPro's MSYS2 CMake assumes the POSIX command-line limit, so it writes no response files, while its tools are native
Windows programs limited to 32,767 characters. The core's archive command listed all of its objects (27,354 characters
in the repository, 35,454 in a game project's `_deps/nnhge-build/`). So the core's sources are the OBJECT library
`nnh_core_objects` (`engine/core/CMakeLists.txt`) and the static `nnh_core` is archived in `engine/core/archive/`, whose
folder sets `CMAKE_NINJA_FORCE_RESPONSE_FILE` in 3DS presets: CMake reads it per folder, so only that archive uses a
response file (paths relative to the build folder, which `arm-none-eabi-gcc-ar` reads). Set for the whole build it
would break every compile: compiler response files would hold `/e/...` include paths the native compiler can't read.
`libnnh_core.a` keeps its place and members; `hello.3dsx` stayed byte-identical.

**The variable is undocumented**: CMake's Ninja generator reads it, but `cmake --help-variable` says it is not
defined, so a CMake update could stop honouring it without an error. The length check below is its guard.

`scripts/check_command_length.sh <build dir> [limit]` prints the longest command Ninja would run (`ninja -t commands`),
its share of 32,767 and its start, and exits 1 above the limit (default 16,383, 50 %): today 2,533 characters (7.7 %,
the platform archive) in both 3DS presets of the repository, 3,145 in a game project at a 78-character path. It
runs at every spec's stage ends on Windows, in `scripts/vm_check.sh final` (the repository's fresh `3ds-dev` and the
consumer copy's 3DS folders) and in spec 034's release checklist. Without targets it reads the default build, which
leaves out `dist` and `<game>_cia`: `scripts/check_command_length.sh build/3ds-dev 16383 dist` measures packaging too.

### Consumer tests

`tests/consumer/` is a game project kept with the tests (FR-16): every `nnh_add_game()` keyword and asset kind in a
nested folder, a hello-sized game, and a plain library of its own. `tests/consumer.cmake` registers, in PC presets:

| ctest | Label | What |
|-------|-------|------|
| `consumer` | `consumer` | the project against this engine folder (`FETCHCONTENT_SOURCE_DIR_NNHGE`): configure, build, run headless, the self-check line, no engine target or test in its build, its manifest, compile commands equal to `hello`'s, its own library untouched, `NNHGE_VERSION` |
| `consumer_platform_empty`, `consumer_platform_unknown` | `consumer` | the unknown-platform error before any target |
| `consumer_archive`, `consumer_archive_override`, `consumer_submodule`, `consumer_pruned`, `consumer_bad_hash`, `consumer_options_on`, `consumer_options_mixed` | `consumer-full` | the other ways of adding the engine, a wrong hash, all four options ON, `NNH_BUILD_TESTS` alone ON |

The driver is `tests/consumer_tools/run_consumer.cmake`; builds go to `build/<preset>/consumer/<way>/` (the
`consumer-full` ones are deleted once they pass) and reuse the parent's compiler cache (`CCACHE_NOHASHDIR=1`) and SDL3
download. `ctest -LE consumer` skips them while developing; `scripts/vm_check.sh final` runs `consumer-full`.

### `nnh`'s template and its ctests (spec 033)

`tools/nnh/CMakeLists.txt` embeds the `nnh new` template: `NNH_TEMPLATE_FILES` lists the files of
`tools/nnh/template/` in the order they are written, and a custom command runs `tools/nnh/embed_template.cmake` on them
(`DEPENDS` on each) into the generated `template_files.cpp` of `nnh` (text with LF line endings, `.png`/`.wav` byte for
byte; the normalisation is done in hex because `file(WRITE)` is a text write on Windows). Two cache entries,
`NNH_RELEASE_ENGINE_URL` and `NNH_RELEASE_ENGINE_SHA256`, are the release pin a release `nnh` writes into new projects
(spec 034 sets them; both or neither, `https://` or `file://`, 64 hex digits, checked by
`tools/nnh/nnh_release_pin.cmake` at configure); they and `NNHGE_VERSION` reach `build_info.cpp` only, so a new pin
recompiles one file. With tests, in PC presets:

| ctest | Label | What |
|-------|-------|------|
| `nnh_pin_check` | — | the pin check on good and bad values (`cmake -P`, 2 s) |
| `nnh_new_files` | — | the built `nnh`: `--version`, `help`, `nnh new` in a scratch folder (the files, refusals, exit codes, same bytes for the same inputs) |
| `nnh_template_smoke` | — | the template's `tests/smoke.nnhs` against `nnh_template_check`, headless (not in Release builds) |
| `nnh_new_smoke` | `consumer-full` | a project made by `nnh new` outside the engine, built with `nnh` and its smoke script run, with the local engine and with the engine packed as a pinned `file://` archive (≈ 4 min on Windows) |

`nnh_template_check` is the template's sources and assets as a game of the engine's own build, compiled with the engine's
warning flags and `-Werror`: a template that no longer compiles fails every PC build with tests. It is in the
repository's games manifest (so `nnh serve` lists it there).

## Rules (Modern CMake)

- Targets and properties only: `target_include_directories`, `target_link_libraries`,
  `target_compile_options`. No `include_directories()`, `link_libraries()`, or editing `CMAKE_CXX_FLAGS`.
- Say `PUBLIC` / `PRIVATE` / `INTERFACE` explicitly. SDL3 is `PRIVATE` to the engine.
- Namespaced aliases for libraries (`nnh::engine`).
- No `file(GLOB)` for sources: explicit lists make added/removed files visible in review and keep Ninja
  exact. (Asset folders are the exception, see `ASSETS` above.)
- Custom functions parse arguments with `cmake_parse_arguments(PARSE_ARGV ...)`.
- C++ standard through `CMAKE_CXX_STANDARD` / `target_compile_features`, never `-std=` flags.
- Build-step scripts run with `cmake -P` (`cmake/nnh_*.cmake` with `-D` arguments) start with
  `cmake_minimum_required(VERSION 3.25)`: a `-P` script doesn't inherit the project's policies. Without
  it, CMake 3.x rejects `if(x IN_LIST y)`; CMake 4 (Windows) silently accepts it, so the bug only shows on
  Linux. Test such scripts with a ctest (`tests/data/asset_lists/`).

## SDL3 for the simulator

`cmake/nnh_sdl3.cmake`: system SDL3 if found → official prebuilt package for MinGW or MSVC (no compile)
→ static build from source (Linux/macOS, needs SDL's [Linux build dependencies](https://wiki.libsdl.org/SDL3/README-linux)).
Downloads are pinned with SHA-256 in `cmake/nnh_sdl3_pin.cmake` (the version, URLs and hashes; `nnh doctor --online`
fetches exactly those URLs, and a unit test keeps its copy equal). If HTTPS fails with "certificate signer not trusted" (WinLibs/MSYS2
CMake builds with GnuTLS and no CA store), the script reuses Git for Windows' CA bundle; otherwise set
`CMAKE_TLS_CAINFO`.

## devkitPro helpers available

From `C:\devkitPro\cmake\Platform\Nintendo3DS.cmake`: `ctr_generate_smdh`, `ctr_create_3dsx`
(`SMDH`, `ROMFS` options), `ctr_add_shader_library` (picasso), `ctr_add_graphics_target` (tex3ds:
`IMAGE`, `ATLAS`, `CUBEMAP`, `SKYBOX`; we call `tex3ds` directly through `CTR_TEX3DS_EXE` instead, to
keep one step per image). Platform flags: `-march=armv6k -mtune=mpcore -mfloat-abi=hard`,
`-D__3DS__`, links `-lctru -lm` by default.

## Build speed

- Ninja + ccache (auto-detected) → incremental game rebuild ≈ 1.4 s PC, 2.6 s 3DS for `hello` (2026-11-22;
  history in the ROADMAP's build-time table). On the maintainer's machine devkitPro's MSYS2 has no ccache, so clean
  3DS builds compile everything.
- The engine is a static library; game builds compile only game sources.
- Keep public headers light (see [cpp.md](cpp.md)). Measure before/after any build change:
  `time cmake --build --preset pc-debug` after touching one game `.cpp`.

## CI

**CI is manual** (maintainer's decision, 2026-11-17, to save Actions minutes): run it from the Actions
tab (CI → Run workflow) before releases or when a Linux/container check is needed. Day-to-day
verification happens locally (Windows: PC + 3DS presets) and on the homelab build VM (Linux).

`.github/workflows/ci.yml`: `docs` (broken links, layering check), `pc-linux` (installs SDL deps,
builds, `ctest`, runs `hello` headless with `SDL_VIDEO_DRIVER=offscreen`) and `nintendo-3ds` (container
`devkitpro/devkitarm`, uploads `.3dsx`). Job ids must start with a letter.

Linux builds and libFuzzer run on the homelab build VM (`ssh nnhge-build`, VM 120, off by default; see
CLAUDE.md "Build" for the start/sync/stop commands). Its devkitPro was extracted from the official
`devkitpro/devkitarm` image (to `/opt/devkitpro`; the image itself was removed to free the disk) because
`*.devkitpro.org` answers 403 to the maintainer's IP. It also has the 32-bit multilib (2026-11-25): a 32-bit host build
checks the engine on a second 32-bit target besides the 3DS. It must use SSE floats, since x87's extended precision
breaks the same-bits rule:

```sh
ssh nnhge-build 'cd ~/wt/nnhge && F="-m32 -msse2 -mfpmath=sse" && cmake --preset pc-debug -B build/m32 -DCMAKE_C_FLAGS="$F" -DCMAKE_CXX_FLAGS="$F" -DCMAKE_EXE_LINKER_FLAGS=-m32 && cmake --build build/m32 --target nnh_tests && build/m32/tests/nnh_tests'
```

`scripts/vm_check.sh` runs the passes (maintainer's decision, 2026-11-27, to shorten each spec's Linux stage):

| Command | What it does | Use |
|---------|--------------|-----|
| `scripts/vm_check.sh sync` | Sends the working tree with each file's own time and deletes files removed locally; the VM's build folders stay, so Ninja and ccache rebuild only what changed | before any manual command |
| `scripts/vm_check.sh fast` | Sync, then unit tests (`nnh_tests`, `nnh_tool_tests`, `nnh_assetc_tests`) on GCC 13 and under clang ASan/UBSan, and the `FUZZ` targets for 2 min each | while fixing findings |
| `scripts/vm_check.sh final` | Sync, then the full ctest on GCC 13 (end-to-end `*_script_pc` tests included), the consumer tests' `consumer-full` ways, the consumer project's 3DS builds through `nnh` from a copy under `/tmp` and a fresh `3ds-dev` `nnh build animation` (spec 032), a project made by `nnh new` under `/tmp` (`nnh doctor` without and with `DEVKITPRO`, its 3DS builds and `.cia` through `nnh`, its smoke script; spec 033), unit tests on GCC 14 and the 32-bit host, ASan/UBSan, and the `FUZZ` targets for 10 min each | once, when the implementation is complete, before the merge |

`FUZZ="tests/fuzz_remote tools/assetc/fuzz_assetc_ttf"` names the targets (paths inside `build/asan`); `CLEAN=1`
wipes the VM's tree and builds first. The end-to-end `*_script_pc` tests already run in the Windows suite, so the
fast pass skips them.

Size budgets written for the console (`static_assert`s and tests) test `nnh::detail::kThreeDsLayout`
(`nnh/detail/target_layout.hpp`: 32-bit pointers and an 8-byte `max_align_t`), not `sizeof(void*) == 4`: 32-bit x86
aligns `max_align_t` to 16 and pads more.

Before pushing, run the same checks locally:

```sh
python -I scripts/docs_tool.py check
cmake --build --preset pc-debug && ctest --preset pc-debug
scripts/dkp.sh cmake --build --preset 3ds-release
```

`.github/workflows/wiki.yml`: on push to `main` touching docs, regenerates the GitHub wiki from the
repository docs and pushes it ([documentation.md](../process/documentation.md)).

## Further reading

- [Effective Modern CMake](https://gist.github.com/mbinna/c61dbb39bca0e4fb7d1f73b0d66a4fd1)
- Henry Schreiner, [Modern CMake](https://cliutils.gitlab.io/modern-cmake/)
- [cmake-presets(7)](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
