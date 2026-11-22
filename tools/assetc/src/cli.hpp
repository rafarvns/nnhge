#pragma once
// The nnh-assetc command line (spec 012 D27):
//
//   nnh-assetc --version
//   nnh-assetc map <in.tmj> <out.nnhmap> --assets <assets folder> [--depfile <out.d>]
//
// `map` converts a Tiled map; on failure it prints `file: reason` (the file the problem is in: the map, a tileset or
// an image), removes any old output and exits with 1. Usage errors exit with 2.

#include <cstdio>

namespace nnh::assetc {

/// Runs the tool on `argv` (argv[0] is the program). Results go to `out`, messages to `err`. Returns the exit code.
int run_cli(int argc, const char* const* argv, std::FILE* out, std::FILE* err);

} // namespace nnh::assetc
