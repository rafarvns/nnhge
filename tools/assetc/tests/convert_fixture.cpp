// Shared by the data converter tests (convert_fixture.hpp).

#include "convert_fixture.hpp"

#include <deque>
#include <memory>

#include "data/data_apply.hpp"
#include "data/data_validate.hpp"
#include "data/schema_fingerprint.hpp"
#include "data/schema_write.hpp"
#include "test.hpp"

namespace convert_test {

namespace {

void append(void* context, const char* text, std::size_t size) {
    static_cast<std::string*>(context)->append(text, size);
}

} // namespace

const nnh::assetc::Schema& schema_of(const nnh::detail::DataRegistry& registry) {
    static std::deque<std::unique_ptr<nnh::assetc::Schema>> kept;
    std::string text;
    nnh::detail::write_schema(registry, append, &text);
    kept.push_back(std::make_unique<nnh::assetc::Schema>());
    std::string error;
    CHECK(nnh::assetc::read_schema(text, *kept.back(), error));
    if (!error.empty()) std::printf("schema: %s\n", error.c_str());
    return *kept.back();
}

const nnh::assetc::Schema& schema() {
    static const nnh::assetc::Schema& s = schema_of(nnh::detail::data_registry());
    return s;
}

Result convert(std::string_view text, const char* key, const nnh::assetc::Schema& s) {
    Result r;
    nnh::assetc::Diagnostics problems;
    const nnh::detail::TypeInfo* type = s.type_by_key(key);
    CHECK(type != nullptr);
    if (type == nullptr) return r;
    r.ok = nnh::assetc::convert_data(s, *type, text, r.data, problems);
    r.problems = problems.text("f");
    return r;
}

bool load_compiled_raw(const std::vector<std::uint8_t>& bytes, const nnh::detail::DataRegistry& registry,
                       const void* tag, void* out) {
    const nnh::detail::TypeInfo* type = nnh::detail::find_type(registry, tag);
    if (type == nullptr) return false;
    const std::uint32_t fingerprint = nnh::detail::schema_fingerprint(registry, *type);
    const auto check = nnh::detail::validate_data(bytes.data(), bytes.size(), registry, *type, fingerprint);
    if (!check.ok()) {
        std::printf("validate: %s\n", check.reason);
        return false;
    }
    type->reset(out);
    nnh::detail::apply_data(bytes.data(), bytes.size(), registry, *type, out);
    return true;
}

} // namespace convert_test
