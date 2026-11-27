// FR-13 in Release (spec 031 T2.6, FR-19): the queries are constants, nothing is published, the event is 8 bytes. In
// Debug/Development builds the same test checks the switch is on.

#include "nnh/assets.hpp"
#include "nnh/build.hpp"
#include "test.hpp"

NNH_TEST(reload_release_queries_are_constants) {
    static_assert(sizeof(nnh::AssetsReloaded) == 8, "AssetsReloaded is 8 bytes");
    if constexpr (nnh::build::kDevToolsEnabled) {
        CHECK(nnh::assets::kReloadEnabled);
        return;
    }
    CHECK(!nnh::assets::kReloadEnabled);
    CHECK(nnh::assets::reload_count() == 0);
    CHECK(!nnh::assets::reloaded("art/hero"));
    CHECK(nnh::assets::reloaded_paths().count == 0);
    CHECK(nnh::assets::reloaded_paths().begin() == nnh::assets::reloaded_paths().end());
}
