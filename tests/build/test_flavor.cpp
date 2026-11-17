// Build flavors (docs/architecture/foundation.md#build-flavors).

#include "nnh/build.hpp"
#include "test.hpp"

NNH_TEST(exactly_one_flavor_macro_is_defined) {
    constexpr int defined_count = 0
#if defined(NNH_FLAVOR_DEBUG)
                                  + 1
#endif
#if defined(NNH_FLAVOR_DEVELOPMENT)
                                  + 1
#endif
#if defined(NNH_FLAVOR_RELEASE)
                                  + 1
#endif
        ;
    CHECK(defined_count == 1);
}

NNH_TEST(flavor_matches_the_flavor_macro) {
#if defined(NNH_FLAVOR_DEBUG)
    CHECK(nnh::build::flavor() == nnh::build::Flavor::Debug);
#elif defined(NNH_FLAVOR_DEVELOPMENT)
    CHECK(nnh::build::flavor() == nnh::build::Flavor::Development);
#else
    CHECK(nnh::build::flavor() == nnh::build::Flavor::Release);
#endif
}

NNH_TEST(asserts_and_dev_tools_are_off_only_in_release) {
    const bool release = nnh::build::flavor() == nnh::build::Flavor::Release;
    CHECK(nnh::build::kAssertsEnabled == !release);
    CHECK(nnh::build::kDevToolsEnabled == !release);
}

NNH_TEST(flavor_has_a_name) {
    const char* name = nnh::build::flavor_name();
    CHECK(name != nullptr && name[0] != '\0');
}
