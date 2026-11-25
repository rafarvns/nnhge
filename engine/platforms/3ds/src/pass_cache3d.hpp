#pragma once
// The 3DS 3D pass's record of the per-draw GPU state it last sent (pass3d_3ds.cpp; spec 026, optimisation round of
// 026a). Each piece is sent the first time in a pass and again only when it changes, so a run of draws of one mesh and
// one material sends only its matrices and the draw: the vertex buffer (citro3d rewrites 40 command words for it on
// every C3D_GetBufInfo), the fixed attributes (6 words each, four f32→f24 conversions), the uv decode and colour scale
// uniforms (8 words together, so the matrices' upload stays one run) and the texture (C3D_TexBind makes citro3d set the
// unit again and clear the GPU's texture cache). Reset at the start of every pass: citro2d changes the same state
// between passes. Pure (no citro3d), tested on the PC (tests/platform_3ds/test_pass_cache3d.cpp).

#include <cstdint>
#include <cstring>

namespace nnh::platform::ctr {

class PassCache3D {
public:
    /// Forgets everything: the next request of each kind is sent.
    void reset() { *this = PassCache3D{}; }

    /// The vertex buffer of `mesh` read with the attribute `layout`: true = bind it (first, or either changed).
    bool vertex_buffer(std::uintptr_t mesh, std::uint8_t layout) {
        if (has_buffer_ && mesh == mesh_ && layout == layout_) return false;
        has_buffer_ = true, mesh_ = mesh, layout_ = layout;
        return true;
    }

    /// The fixed value of attribute `id` (0..7; the pass fixes 2 = uv and 3 = colour to constants): true the first
    /// time in the pass. Fixed values stay on the GPU while other layouts read the attribute from the buffer.
    bool fixed_attribute(int id) {
        const std::uint8_t bit = static_cast<std::uint8_t>(1u << (id & 7));
        if ((fixed_ & bit) != 0) return false;
        fixed_ = static_cast<std::uint8_t>(fixed_ | bit);
        return true;
    }

    /// The per-draw constants other than the matrices (uv scale and offset, base colour: 8 floats): true = upload
    /// (first, or any bit changed — compared as bits, so the GPU always holds exactly what the draw asked for).
    bool constants(const float values[8]) {
        if (has_constants_ && std::memcmp(values, constants_, sizeof constants_) == 0) return false;
        has_constants_ = true;
        std::memcpy(constants_, values, sizeof constants_);
        return true;
    }

    /// A texture with its sampling parameters (filter and wraps, packed by the caller): true = set them and bind.
    bool texture(std::uintptr_t texture, std::uint32_t params) {
        if (has_texture_ && texture == texture_ && params == params_) return false;
        has_texture_ = true, texture_ = texture, params_ = params;
        return true;
    }

private:
    std::uintptr_t mesh_ = 0;
    std::uintptr_t texture_ = 0;
    std::uint32_t params_ = 0;
    float constants_[8] = {};
    std::uint8_t layout_ = 0;
    std::uint8_t fixed_ = 0; ///< bit per attribute already fixed in this pass
    bool has_buffer_ = false;
    bool has_constants_ = false;
    bool has_texture_ = false;
};

} // namespace nnh::platform::ctr
