#pragma once
// The commands of the nnh-assetc command line (cli.hpp), one file each. Internal.

#include <cstdio>

namespace nnh::assetc {

/// `data <in> <out> --schema <description> --refs <sidecar> --name <asset name> [--show <path shown>]` (spec
/// 014): returns the exit code (2 for a usage error, which run_cli reports).
int run_data(int argc, const char* const* argv, std::FILE* err);

/// `data-index --out <.nnh_data> --schema <description> --assets-list <names> --refs-list <sidecar paths>
/// [--show <assets folder shown>]` (spec 014): returns the exit code (2 for a usage error).
int run_data_index(int argc, const char* const* argv, std::FILE* err);

/// `font <in.font|in.fnt> <out.nnhfont> --assets <folder> --show <asset name> [--depfile <out.d>]
/// [--strings <code>=<file>]...` (spec 029): the report line and notes go to `out`; returns the exit code (2: usage).
int run_font(int argc, const char* const* argv, std::FILE* out, std::FILE* err);

/// `strings <code> --languages <list> --out <out.nnhstr> [--release] [--strict]` and `strings-index --languages <list>
/// --out <.nnh_loc> [--fonts <converted assets>] [--pseudo-out <qps.nnhstr>] [--release] [--strict]` (spec 029).
int run_strings(int argc, const char* const* argv, std::FILE* out, std::FILE* err);
int run_strings_index(int argc, const char* const* argv, std::FILE* out, std::FILE* err);

} // namespace nnh::assetc
