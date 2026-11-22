// nnh-assetc — NNHGE's asset converter (spec 012, decision D27): a host program run by the build. The command line
// lives in cli.cpp (tested); this file only forwards to it.

#include <cstdio>

#include "cli.hpp"

int main(int argc, char** argv) { return nnh::assetc::run_cli(argc, argv, stdout, stderr); }
