// The converted file around the values and the references sidecar (spec 014 T5.3, FR-10): header, CRC and
// fingerprint, the 64 KiB limit, the engine's validator on every file written, and the sidecar's text.

#include <string>
#include <vector>

#include "convert_fixture.hpp"
#include "core/crc32.hpp"
#include "data/data_format.hpp"
#include "data_refs.hpp"
#include "data_writer.hpp"
#include "test.hpp"

using nnh::assetc::write_data;
namespace fmt = nnh::detail::datafmt;

namespace {

std::uint32_t u32_at(const std::vector<std::uint8_t>& f, std::size_t at) {
    return f[at] | (f[at + 1] << 8) | (f[at + 2] << 16) | (static_cast<std::uint32_t>(f[at + 3]) << 24);
}

const nnh::detail::TypeInfo& item() {
    return *convert_test::schema().type_by_key("item");
}

} // namespace

NNH_TEST(data_writer_puts_the_header_crc_and_fingerprint_around_the_values) {
    const std::vector<std::uint8_t> body = {1, 1, 5, 0, 0, 0}; // price = 5
    std::vector<std::uint8_t> file;
    std::string problem;
    CHECK(write_data(convert_test::schema(), item(), body, file, problem));
    CHECK(problem.empty() && file.size() == fmt::kHeaderSize + body.size());
    CHECK(file[0] == 'N' && file[1] == 'N' && file[2] == 'H' && file[3] == 'D');
    CHECK(file[fmt::kVersionAt] == 1 && file[fmt::kVersionAt + 1] == 0);
    CHECK(file[fmt::kFlagsAt] == 0 && file[fmt::kFlagsAt + 1] == 0);
    CHECK(u32_at(file, fmt::kSizeAt) == file.size());
    CHECK(u32_at(file, fmt::kCrcAt) == nnh::detail::crc32(body.data(), body.size()));
    CHECK(u32_at(file, fmt::kFingerprintAt) == convert_test::schema().fingerprint(item()));
    CHECK(std::vector<std::uint8_t>(file.begin() + fmt::kHeaderSize, file.end()) == body);
}

NNH_TEST(data_writer_refuses_a_file_over_64_kib) {
    std::vector<std::uint8_t> file;
    std::string problem;
    const std::vector<std::uint8_t> body(fmt::kMaxFileBytes - fmt::kHeaderSize + 1, 0);
    CHECK(!write_data(convert_test::schema(), item(), body, file, problem));
    CHECK(problem == "the converted file would be 65537 bytes: at most 64 KiB");
}

NNH_TEST(data_writer_checks_its_output_with_the_engines_validator) {
    std::vector<std::uint8_t> file;
    std::string problem;
    const std::vector<std::uint8_t> body = {1, 9, 0}; // field 9 of a 3-field type
    CHECK(!write_data(convert_test::schema(), item(), body, file, problem));
    CHECK(problem ==
          "internal error: the converted file fails the engine's check (field indices out of order or out "
          "of range)");
}

NNH_TEST(data_refs_sidecar_lists_the_asset_and_its_references) {
    const auto r = convert_test::convert(
        "name = \"Gob\"\nsprite = \"art/gob lin\"\n[[drops]]\nitem = \"items/potion\"\n");
    CHECK(r.ok);
    CHECK(nnh::assetc::refs_text("enemies/goblin", "enemy", 123, r.data.references) ==
          "nnh-data-refs 1\n"
          "asset enemy 123 enemies/goblin\n"
          "ref 2 texture - sprite art/gob lin\n"
          "ref 4 data item drops[0].item items/potion\n");
}

NNH_TEST(data_refs_sidecar_of_a_file_that_isnt_a_data_asset_is_empty) {
    CHECK(nnh::assetc::empty_refs_text() == "nnh-data-refs 1\n");
}
