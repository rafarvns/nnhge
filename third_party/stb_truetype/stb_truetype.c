// stb_truetype (see README.md): the implementation, compiled once for nnh-assetc only (host). Its asserts are off: the
// buffer helpers clamp bad offsets themselves, and ttf_check.cpp refuses what they cannot (fuzzed).
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_assert(x) ((void)0)
#include "stb_truetype.h"
