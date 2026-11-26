// nnh::Font (nnh/font.hpp) and FontRef's conversion: queries over the font table, the release of the last owner.

#include "nnh/font.hpp"

#include "font/font_internal.hpp"
#include "font/font_table.hpp"
#include "nnh/build.hpp"
#include "text/font_resolve.hpp"

namespace nnh {

FontRef::FontRef(const Font& font) : id(FontAccess::id(font)) {}

Font::~Font() { detail::font_release(id_); }

Font::Font(Font&& other) noexcept : id_(other.id_) { other.id_ = 0; }

Font& Font::operator=(Font&& other) noexcept {
    if (this != &other) {
        detail::font_release(id_);
        id_ = other.id_;
        other.id_ = 0;
    }
    return *this;
}

bool Font::valid() const { return id_ != 0 && detail::font_table_slot(id_) >= 0; }

FontRef Font::ref() const { return FontRef(*this); }

FontMetrics Font::metrics() const { return gfx::font_metrics(ref()); }

bool Font::has(char32_t cp) const {
    const detail::FontBlock* f = valid() ? detail::font_table_block(id_) : nullptr;
    return f != nullptr && detail::find_glyph(*f, cp) >= 0;
}

bool Font::covers(char32_t cp) const {
    detail::TextFace face;
    if (!valid() || !detail::font_resolve_face(id_, face)) return false;
    for (int i = 0; i < face.count; ++i)
        if (detail::find_glyph(*face.fonts[i], cp) >= 0) return true;
    return false;
}

int Font::advance(char32_t cp) const {
    detail::TextFace face;
    if (valid()) detail::font_resolve_face(id_, face);
    return detail::face_glyph(face, cp).advance;
}

int Font::kerning(char32_t left, char32_t right) const {
    detail::TextFace face;
    if (!valid() || !detail::font_resolve_face(id_, face)) return 0;
    return detail::face_kerning(face, detail::face_glyph(face, left), detail::face_glyph(face, right));
}

const char* Font::path() const {
    if constexpr (!build::kDevToolsEnabled) return "";
    return valid() ? detail::font_table_extra(detail::font_table_slot(id_)).path : "";
}

} // namespace nnh
