// A world's life (spec 020 FR-9, FR-23): destructor (clear with hooks, then every object destroyed and the block
// freed), moves, misuse reports, the end of the outermost iteration, the checked accessor's scratch slot. A world
// destroyed inside its own iteration or hook (misuse) is freed by the outermost operation, without more hooks.

#include "nnh/assert.hpp"
#include "entities/world_internal.hpp"

NNH_LOG_CHANNEL(entities);

namespace nnh {
namespace detail {

void world_misuse(const char* what) {
    NNH_LOG_ERROR(entities, "%s", what);
    NNH_ASSERT(false, "%s", what);
}

void world_teardown(WorldHot* w) {
    for (int k = 0; k < w->kind_count; ++k) {
        void (*destroy)(void*) = w->cold->kinds[k].destroy;
        if (destroy == nullptr) continue;
        for (int s = 0; s <= w->kinds[k].slots.capacity; ++s) destroy(component_at(*w, k, s));
    }
    mem::free(w);
}

bool world_finish(WorldHot& w) {
    if ((w.pending & kPendingDoomed) == 0 || world_busy(w)) return false;
    world_teardown(&w);
    return true;
}

void world_end_iteration(WorldHot& w) {
    if ((w.pending & kPendingFresh) != 0) {
        w.entities.end_iteration();
        for (int k = 0; k < w.kind_count; ++k) w.kinds[k].slots.end_iteration();
        w.pending = static_cast<std::uint8_t>(w.pending & ~kPendingFresh);
    }
    world_finish(w);
}

void* world_scratch(WorldHot& w, int kind) {
    const int s = w.kinds[kind].slots.capacity;
    unsigned char* p = component_at(w, kind, s);
    if (void (*destroy)(void*) = w.cold->kinds[kind].destroy) destroy(p);
    w.cold->kinds[kind].construct(p);
    return p;
}

} // namespace detail

World::~World() {
    detail::WorldHot* w = hot_;
    if (w == nullptr) return;
    if (detail::world_busy(*w)) {
        detail::world_misuse("nnh::World destroyed inside its own iteration or remove hook");
        hot_ = nullptr;
        detail::world_unbind(*w);
        w->pending = static_cast<std::uint8_t>(w->pending | detail::kPendingDoomed); // freed when it ends
        return;
    }
    detail::world_unbind(*w);
    detail::world_clear(*this, *w); // hooks see a working world
    hot_ = nullptr;
    detail::world_teardown(w);
}

World::World(World&& other) noexcept {
    if (other.hot_ != nullptr && other.hot_->cold->scene != nullptr) {
        detail::world_misuse("moving a bound nnh::World (nothing moves): keep it in its scene");
        return;
    }
    hot_ = other.hot_;
    other.hot_ = nullptr;
}

World& World::operator=(World&& other) noexcept {
    if (this == &other) return *this;
    if (other.hot_ != nullptr && other.hot_->cold->scene != nullptr) {
        detail::world_misuse("moving a bound nnh::World (nothing moves): keep it in its scene");
        return *this;
    }
    detail::WorldHot* mine = hot_;
    if (mine != nullptr && detail::world_busy(*mine)) // Debug stops here, the world untouched
        detail::world_misuse("nnh::World replaced inside its own iteration or remove hook");
    hot_ = other.hot_;
    other.hot_ = nullptr;
    if (mine != nullptr) {
        World old; // its destructor unbinds, clears (hooks run) and frees the replaced block
        old.hot_ = mine;
    }
    return *this;
}

} // namespace nnh
