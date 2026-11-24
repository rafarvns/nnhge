// <nnh/particles.hpp> on its own (spec 024 stage 9, Linux): GCC 13 crashed (internal compiler error) on every braced
// `draw(screen, {.layer = n})` while Texture was only declared, because overload resolution tried the designated list
// against the incomplete `const Texture&` of the textured draw(). The header now includes <nnh/texture.hpp>.
// Keep <nnh/particles.hpp> the first include of this file.

#include "nnh/particles.hpp"

#include "test.hpp"

static_assert(sizeof(nnh::Texture) > 0, "<nnh/particles.hpp> must make Texture complete (GCC 13 braced draw ICE)");

template <typename E>
concept DrawsWithBracedOptions = requires(const E& e) {
    e.draw(nnh::Screen::Top, {.layer = 2});
    e.draw(nnh::Screen::Top, {.layer = 5, .ignore_camera = true});
};
static_assert(DrawsWithBracedOptions<nnh::ParticleEmitter>, "draw() must accept braced options on every compiler");

NNH_TEST(particles_header_alone_draws_with_braced_options) {
    const nnh::ParticleEmitter empty;
    CHECK(!empty.valid()); // compile-time checks above; nothing to run
}
