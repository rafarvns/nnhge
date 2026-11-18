#pragma once
// Declarations of the vendored stb_vorbis (third_party/stb_vorbis), with the same configuration it is
// compiled with. Only audio/ogg_reader.cpp includes this.

#define STB_VORBIS_HEADER_ONLY
#define STB_VORBIS_NO_PULLDATA_API
#include "stb_vorbis.c"
#undef STB_VORBIS_HEADER_ONLY
