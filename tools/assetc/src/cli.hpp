#pragma once
// The nnh-assetc command line (spec 012 D27):
//
//   nnh-assetc --version
//   nnh-assetc map <in.tmj> <out.nnhmap> --assets <assets folder> [--depfile <out.d>]
//   nnh-assetc data <in> <out> --schema <schema.nnhschema> --refs <out.refs> --name <asset name> [--show <path>]
//
// `map` converts a Tiled map; on failure it prints `file: reason` (the file the problem is in: the map, a tileset or
// an image), removes any old output and exits with 1. `data` (spec 014) converts a data file whose extension is a
// key of the schema description and copies any other asset unchanged, writing a references sidecar either way; on
// failure it prints `<shown path>:<line>: reason` (up to 20), removes any old outputs and exits with 1. Usage
// errors exit with 2.

#include <cstdio>

namespace nnh::assetc {

/// Runs the tool on `argv` (argv[0] is the program). Results go to `out`, messages to `err`. Returns the exit code.
int run_cli(int argc, const char* const* argv, std::FILE* out, std::FILE* err);

} // namespace nnh::assetc
