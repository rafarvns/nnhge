// face_glyph's out-of-line part (text_face.hpp): kept out of the per-glyph loops so their common case stays small.

#include "text/text_face.hpp"

namespace nnh::detail {

FaceGlyph face_glyph_slow(const TextFace& f, char32_t cp) { return face_glyph_full(f, cp); }

} // namespace nnh::detail
