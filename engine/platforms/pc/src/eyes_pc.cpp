// PC simulator: stereo (contract nnh/platform/video3d.hpp set_stereo/select_eye, spec 026 FR-14/FR-22). The right eye
// is a second top-screen target the size of the first; select_eye swaps ctx.screens[0] between them, so every top-screen
// draw (2D, tiles, the 3D pass and its read-back) goes to the selected eye unchanged. When the window is composed, the
// top screen shows the eye view F8 picked (NNH_SIM_EYE): the left eye, the right one, or a red/cyan anaglyph (left ×
// red + right × cyan, added by the renderer: a preview, not golden). Linked only with 3D (video3d_pc.cpp).

#include <SDL3/SDL.h>

#include "context_pc.hpp"
#include "nnh/platform/video3d.hpp"
#include "video3d_pc.hpp"

namespace nnh::platform {
namespace {

struct Eyes {
    SDL_Texture* left = nullptr;  // the top screen's own target (ctx.screens[0] outside a stereo sequence)
    SDL_Texture* right = nullptr; // made at the first stereo frame
    bool stereo = false;          // this frame draws two eyes
    bool right_cleared = false;
};
Eyes g;

void draw_eye(SDL_Renderer* r, SDL_Texture* eye, const SDL_FRect& dest, Uint8 red, Uint8 green_blue, SDL_BlendMode m) {
    SDL_SetTextureColorMod(eye, red, green_blue, green_blue);
    SDL_SetTextureBlendMode(eye, m);
    SDL_RenderTexture(r, eye, nullptr, &dest);
    SDL_SetTextureColorMod(eye, 255, 255, 255);
    SDL_SetTextureBlendMode(eye, SDL_BLENDMODE_BLEND);
}

/// compose_screens()'s top screen (context_pc.hpp compose_top).
void compose_top(const SDL_FRect& dest) {
    pc::Context& ctx = pc::context();
    SDL_Texture* left = g.left != nullptr ? g.left : ctx.screens[0];
    if (!g.stereo || g.right == nullptr || ctx.eye_view == pc::SimEye::Left) {
        SDL_RenderTexture(ctx.renderer, left, nullptr, &dest);
    } else if (ctx.eye_view == pc::SimEye::Right) {
        SDL_RenderTexture(ctx.renderer, g.right, nullptr, &dest);
    } else {
        draw_eye(ctx.renderer, left, dest, 255, 0, SDL_BLENDMODE_NONE);
        draw_eye(ctx.renderer, g.right, dest, 0, 255, SDL_BLENDMODE_ADD);
    }
}

} // namespace

void set_stereo(bool on) {
    pc::Context& ctx = pc::context();
    if (g.left == nullptr) g.left = ctx.screens[0];
    ctx.compose_top = &compose_top;
    if (on && g.right == nullptr) {
        g.right = SDL_CreateTexture(ctx.renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, 400, 240);
        if (g.right != nullptr) SDL_SetTextureScaleMode(g.right, SDL_SCALEMODE_NEAREST);
    }
    g.stereo = on && g.right != nullptr;
    g.right_cleared = false;
    ctx.right_eye = g.stereo ? g.right : nullptr; // read back by remote_capture_right after the frame
}

void select_eye(std::uint8_t eye, Color top_clear) {
    pc::Context& ctx = pc::context();
    if (!g.stereo) return;
    ctx.screens[0] = eye == 1 ? g.right : g.left;
    if (eye == 1 && !g.right_cleared) {
        g.right_cleared = true;
        pc::draw_on(Screen::Top);
        pc::set_draw_color(top_clear);
        SDL_RenderClear(ctx.renderer);
        SDL_SetRenderDrawBlendMode(ctx.renderer, SDL_BLENDMODE_BLEND);
    }
}

void pc::eyes_shutdown() {
    Context& ctx = context();
    if (g.left != nullptr) ctx.screens[0] = g.left;
    SDL_DestroyTexture(g.right); // null-safe
    ctx.compose_top = nullptr;
    ctx.right_eye = nullptr;
    g = Eyes{};
}

} // namespace nnh::platform
