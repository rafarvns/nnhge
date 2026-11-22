// Must not compile (spec 014 FR-1; ctest data_schema_rejects_good / data_schema_explains_good): control: a correct schema compiles (ctest data_schema_accepts_good).
#include <cstdint>
#include <string>

#include "nnh/data.hpp"

enum class Element : std::uint8_t { Normal, Fire };
struct Item {
    nnh::data::String<8> name;
    int price = 1;
    Element element = Element::Normal;
};
NNH_DATA_ENUM(Element, Normal, Fire);
NNH_DATA_TYPE(Item, "item", NNH_DATA_FIELD(name), NNH_DATA_FIELD(price, .min = 0), NNH_DATA_FIELD(element));
