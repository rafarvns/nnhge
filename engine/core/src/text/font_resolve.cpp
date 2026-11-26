// How gfx and the UI reach game fonts (font_resolve.hpp).

#include "text/font_resolve.hpp"

#include "nnh/assert.hpp"
#include "text/layout.hpp"

namespace nnh::detail {

FontResolve g_font_resolve = nullptr;

TextFace face_for(FontRef font, int scale) {
    TextFace face;
    face.scale = clamp_text_scale(scale);
    if (font.id == 0 || g_font_resolve == nullptr) return face;
    if (!g_font_resolve(font.id, face)) {
        NNH_ASSERT(false, "text drawn with a font that was destroyed (stale FontRef)");
        face = TextFace(); // parentheses: GCC 13 ICEs on brace value-init of this struct (gimplify.cc:774)
        face.scale = clamp_text_scale(scale);
    }
    return face;
}

} // namespace nnh::detail
