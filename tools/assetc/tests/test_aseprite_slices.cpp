// The `pivot` slice of Aseprite exports (spec 016 T7.3): the key's pivot relative to its bounds, or the bounds'
// bottom-centre; other slices ignored; no slice, no pivot (the library's top-left default).

#include <string>

#include "aseprite_fixture.hpp"
#include "test.hpp"

using aseprite_fixture::Export;
using nnh::assetc::Problem;
using nnh::assetc::SpriteModel;

namespace {

SpriteModel sliced(const std::string& slices, bool* ok = nullptr, std::string* error = nullptr) {
    Export e = aseprite_fixture::row(2);
    e.slices = slices;
    SpriteModel m;
    Problem p;
    const bool read = aseprite_fixture::read(e, m, p);
    if (ok != nullptr) *ok = read;
    if (error != nullptr) *error = nnh::assetc::problem_text(p);
    return m;
}

} // namespace

NNH_TEST(aseprite_pivot_slice_with_a_pivot_point) {
    const SpriteModel m = sliced(R"({"name": "pivot", "color": "#0000ffff", "keys": [{"frame": 0,
        "bounds": {"x": 4, "y": 10, "w": 8, "h": 6}, "pivot": {"x": 4, "y": 5}}]})");
    CHECK(m.has_pivot && m.pivot_x == 8 && m.pivot_y == 15); // bounds origin + pivot
}

NNH_TEST(aseprite_pivot_slice_without_a_pivot_uses_its_bottom_centre) {
    const SpriteModel m = sliced(R"({"name": "hitbox", "keys": [{"frame": 0, "bounds": {"x": 0, "y": 0, "w": 2, "h": 2}}]},
        {"name": "pivot", "keys": [{"frame": 0, "bounds": {"x": 3, "y": 2, "w": 9, "h": 14}}]})");
    CHECK(m.has_pivot && m.pivot_x == 7.5 && m.pivot_y == 16);
}

NNH_TEST(aseprite_without_a_pivot_slice_has_no_pivot) {
    CHECK(!sliced("").has_pivot);
    CHECK(!sliced(R"({"name": "hitbox", "keys": []})").has_pivot);
}

NNH_TEST(aseprite_pivot_slice_refusals) {
    bool ok = true;
    std::string error;
    sliced(R"({"name": "pivot", "keys": []})", &ok, &error);
    CHECK(!ok && error.find("slice \"pivot\" has no keys") != std::string::npos);
    sliced(R"({"name": "pivot", "keys": [{"frame": 0}]})", &ok, &error);
    CHECK(!ok && error.find("\"bounds\" is missing") != std::string::npos);
    sliced(R"({"name": "pivot", "keys": [{"bounds": {"x": 0, "y": 0, "w": 1, "h": 1}, "pivot": {"x": 0.5, "y": 1}}]})",
           &ok, &error);
    CHECK(!ok && error.find("\"x\" must be a whole number") != std::string::npos);
}
