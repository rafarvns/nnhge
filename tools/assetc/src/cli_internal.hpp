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

} // namespace nnh::assetc
