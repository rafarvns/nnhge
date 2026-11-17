// Per-type ids without RTTI (nnh/detail/type_id.hpp).

#include "nnh/detail/type_id.hpp"
#include "test.hpp"

namespace {

struct Alpha {
    int x;
};
struct Beta {
    int x;
};

} // namespace

NNH_TEST(each_type_has_its_own_id) {
    CHECK(nnh::detail::type_id<Alpha>() != nnh::detail::type_id<Beta>());
    CHECK(nnh::detail::type_id<Alpha>() != nnh::detail::type_id<int>());
}

NNH_TEST(the_same_type_always_has_the_same_id) {
    CHECK(nnh::detail::type_id<Alpha>() == nnh::detail::type_id<Alpha>());
    CHECK(nnh::detail::type_id<Alpha>() != nullptr);
}

NNH_TEST(const_and_plain_types_share_an_id) {
    CHECK(nnh::detail::type_id<const Alpha>() == nnh::detail::type_id<Alpha>());
}

NNH_TEST(ids_are_usable_at_compile_time) {
    constexpr nnh::detail::TypeId id = nnh::detail::type_id<Beta>();
    CHECK(id == nnh::detail::type_id<Beta>());
}
