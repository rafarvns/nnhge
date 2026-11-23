// Layer bits and the detection rule (spec 019 FR-1, FR-2).

#include "nnh/build.hpp"
#include "nnh/collision.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace collision = nnh::collision;
using collision::Layers;

namespace {

// A game's layer enum, as the docs show it.
enum GameLayers : Layers {
    kWorld = collision::kDefaultLayer,
    kPlayer = collision::layer(1),
    kCoin = collision::layer(2),
    kEnemy = collision::layer(30),
};

static_assert(collision::layer(0) == 1u);
static_assert(collision::layer(5) == 32u);
static_assert(collision::layer(31) == 0x80000000u);
static_assert(collision::kNoLayers == 0u && collision::kAllLayers == 0xFFFFFFFFu);
static_assert(collision::kDefaultLayer == collision::layer(0));
static_assert(collision::detects(kPlayer | kCoin, kCoin));
static_assert(sizeof(Layers) == 4);

volatile int g_layer = 0;
Layers g_result = 1;
void make_layer() {
    g_result = collision::layer(g_layer);
}

} // namespace

NNH_TEST(each_layer_number_is_its_own_bit) {
    Layers all = 0;
    for (int n = 0; n < 32; ++n) {
        const Layers bit = collision::layer(n);
        CHECK(bit != 0 && (bit & (bit - 1)) == 0); // one bit
        CHECK((all & bit) == 0);
        all |= bit;
    }
    CHECK(all == collision::kAllLayers);
}

NNH_TEST(a_layer_out_of_range_asserts_and_is_no_layer) {
    for (const int n : {-1, 32, 1000}) {
        g_layer = n;
        g_result = 1;
        CHECK(nnh::platform_fake::run_expecting_crash(make_layer) == nnh::build::kAssertsEnabled);
        if (!nnh::build::kAssertsEnabled) CHECK(g_result == collision::kNoLayers);
    }
}

NNH_TEST(detects_needs_a_shared_bit) {
    CHECK(collision::detects(kPlayer, kPlayer));
    CHECK(collision::detects(kPlayer | kCoin, kCoin | kEnemy));
    CHECK(!collision::detects(kPlayer, kCoin));
    CHECK(collision::detects(collision::kAllLayers, kEnemy));
    CHECK(collision::detects(kWorld, collision::kDefaultLayer));
}

NNH_TEST(no_layers_is_never_detected_and_an_empty_mask_detects_nothing) {
    CHECK(!collision::detects(collision::kAllLayers, collision::kNoLayers));
    CHECK(!collision::detects(collision::kNoLayers, collision::kAllLayers));
}

NNH_TEST(detection_is_one_way_and_the_symmetric_rule_is_one_line) {
    struct Thing {
        Layers layers, mask;
    };
    const Thing player{kPlayer, kCoin | kWorld};
    const Thing coin{kCoin, collision::kNoLayers};        // coins react to nothing
    CHECK(collision::detects(player.mask, coin.layers));  // the player collects the coin
    CHECK(!collision::detects(coin.mask, player.layers)); // the coin doesn't see the player
    const bool symmetric =
        collision::detects(player.mask, coin.layers) && collision::detects(coin.mask, player.layers);
    CHECK(!symmetric);
}

NNH_TEST(game_layer_enums_combine_with_bit_operators) {
    const Layers mask = kPlayer | kWorld;
    CHECK(mask == 3u);
    CHECK((mask & ~kWorld) == kPlayer);
    CHECK(collision::detects(mask ^ kPlayer, kWorld));
}
