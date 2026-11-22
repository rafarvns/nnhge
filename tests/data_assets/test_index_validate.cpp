// The data index validator (spec 014 T3.1, FR-12, FR-14): every well-formed `.nnh_data` is accepted, and each
// way a file can be damaged or over a limit is refused with its own reason, before any lookup reads it.

#include <cstdio>
#include <cstring>
#include <string>

#include "data/index_validate.hpp"
#include "data_file.hpp"
#include "test.hpp"

using data_file::Bytes;
using data_file::IndexSpec;
using nnh::detail::DataProblem;
using nnh::detail::validate_index;
namespace fmt = nnh::detail::idxfmt;

namespace {

IndexSpec sample() {
    IndexSpec s;
    s.types = {{"enemy", 0x11111111u, {"enemies/bat", "enemies/goblin"}},
               {"item", 0x22222222u, {"items/potion"}},
               {"settings", 0x33333333u, {}}};
    s.media[0] = {"art/bat", "art/goblin"};
    s.media[1] = {"sfx/hurt"};
    s.media[2] = {"music/theme"};
    s.largest = 120;
    return s;
}

const char* reason(const Bytes& file) {
    return validate_index(file.data(), file.size()).reason;
}

/// The sample with one byte changed at `at`, size and CRC fixed.
Bytes damaged(std::size_t at, std::uint8_t value, const IndexSpec& spec = sample()) {
    Bytes file = data_file::index_bytes(spec);
    file[at] = value;
    data_file::seal_index(file);
    return file;
}

Bytes with32(std::size_t at, std::uint32_t value, const IndexSpec& spec = sample()) {
    Bytes file = data_file::index_bytes(spec);
    data_file::put32(file, at, value);
    data_file::seal_index(file);
    return file;
}

bool refused_with(const Bytes& file, const char* text) {
    const auto check = validate_index(file.data(), file.size());
    return check.problem == DataProblem::Damaged && std::strstr(check.reason, text) != nullptr;
}

} // namespace

NNH_TEST(index_validator_accepts_a_well_formed_index) {
    const Bytes file = data_file::index_bytes(sample());
    CHECK(validate_index(file.data(), file.size()).ok());
}

NNH_TEST(index_validator_accepts_an_index_without_types_or_entries) {
    const Bytes file = data_file::index_bytes({});
    CHECK(file.size() == fmt::kHeaderSize);
    CHECK(validate_index(file.data(), file.size()).ok());
}

NNH_TEST(index_validator_refuses_damaged_headers) {
    Bytes file = data_file::index_bytes(sample());
    CHECK(refused_with(Bytes(file.begin(), file.begin() + 31), "too short"));
    CHECK(validate_index(nullptr, 0).problem == DataProblem::Damaged);
    CHECK(refused_with(damaged(0, 'X'), "not a data index"));
    Bytes v0 = file;
    data_file::put16(v0, fmt::kVersionAt, 0);
    CHECK(refused_with(v0, "not a data index"));
    Bytes v2 = file;
    data_file::put16(v2, fmt::kVersionAt, 2);
    CHECK(validate_index(v2.data(), v2.size()).problem == DataProblem::NewerVersion);
    Bytes flags = file;
    flags[fmt::kFlagsAt] = 1;
    CHECK(validate_index(flags.data(), flags.size()).problem == DataProblem::NewerVersion);
    Bytes size = file;
    size.push_back(0);
    CHECK(refused_with(size, "size doesn't match"));
    Bytes crc = file;
    crc.back() ^= 1;
    CHECK(refused_with(crc, "CRC mismatch"));
    CHECK(refused_with(damaged(fmt::kReservedAt, 1), "reserved"));
}

NNH_TEST(index_validator_refuses_an_index_over_its_size_limit) {
    Bytes file = data_file::index_bytes({});
    file.resize(fmt::kMaxFileBytes + 1, 0);
    data_file::seal_index(file);
    CHECK(validate_index(file.data(), file.size()).problem == DataProblem::OverLimit);
}

NNH_TEST(index_validator_refuses_counts_over_their_limits) {
    Bytes file = data_file::index_bytes(sample());
    Bytes types = file;
    data_file::put16(types, fmt::kTypeCountAt, fmt::kMaxTypes + 1);
    data_file::seal_index(types);
    CHECK(refused_with(types, "too many types"));
    Bytes data = file;
    data_file::put16(data, fmt::kGroupCountsAt, fmt::kMaxDataEntries + 1);
    data_file::seal_index(data);
    CHECK(refused_with(data, "too many data assets"));
    Bytes media = file;
    data_file::put16(media, fmt::kGroupCountsAt + 2, 2000);
    data_file::put16(media, fmt::kGroupCountsAt + 4, 2000);
    data_file::put16(media, fmt::kGroupCountsAt + 6, fmt::kMaxMediaEntries - 4000 + 1);
    data_file::seal_index(media);
    CHECK(refused_with(media, "too many media"));
    CHECK(refused_with(with32(fmt::kLargestAt, 64 * 1024 + 1), "largest data asset"));
}

NNH_TEST(index_validator_refuses_sections_that_dont_fit) {
    Bytes file = data_file::index_bytes(sample());
    data_file::put16(file, fmt::kGroupCountsAt + 2, 200); // 200 textures: past the end
    data_file::seal_index(file);
    CHECK(refused_with(file, "sections don't fit"));
    Bytes strings = data_file::index_bytes({});
    strings.resize(fmt::kHeaderSize + fmt::kMaxStringBytes + 1, 0);
    data_file::seal_index(strings);
    CHECK(refused_with(strings, "strings over 256 KiB"));
}

NNH_TEST(index_validator_refuses_strings_that_dont_end_with_a_nul) {
    Bytes file = data_file::index_bytes(sample());
    file.back() = 'x';
    data_file::seal_index(file);
    CHECK(refused_with(file, "strings"));
    Bytes extra = data_file::index_bytes({});
    extra.push_back(0); // strings but no entries
    data_file::seal_index(extra);
    CHECK(refused_with(extra, "strings"));
}

NNH_TEST(index_validator_refuses_bad_type_keys) {
    const std::size_t key = data_file::index_types_at();
    CHECK(refused_with(damaged(key, 'E'), "type key"));      // uppercase
    CHECK(refused_with(damaged(key, '1'), "type key"));      // starts with a digit
    CHECK(refused_with(damaged(key, 0), "type key"));        // empty
    CHECK(refused_with(damaged(key + 2, '-'), "type key"));  // a character keys don't use
    CHECK(refused_with(damaged(key + 10, 'x'), "type key")); // garbage after the NUL
    IndexSpec same = sample();
    same.types[1].key = "enemy";
    CHECK(refused_with(data_file::index_bytes(same), "type key"));
    IndexSpec full = sample();
    full.types[2].key = "abcdefghijklmnop"; // 16 characters: no NUL, still valid
    const Bytes ok = data_file::index_bytes(full);
    CHECK(validate_index(ok.data(), ok.size()).ok());
}

NNH_TEST(index_validator_refuses_types_that_dont_tile_the_data_entries) {
    const std::size_t second = data_file::index_types_at() + fmt::kTypeSize;
    CHECK(refused_with(with32(second + 20, 1), "type entries"));           // item's first entry
    CHECK(refused_with(with32(second + 24, 2), "type entries"));           // item's count: past the data
    CHECK(refused_with(with32(second + 24, 0), "type entries"));           // a data entry with no type
    CHECK(refused_with(with32(second + 24, 0xFFFFFFFFu), "type entries")); // overflow
}

NNH_TEST(index_validator_refuses_bad_paths) {
    const IndexSpec s = sample();
    const std::size_t entry = data_file::index_entries_at(s);
    const std::size_t strings = data_file::index_strings_at(s);
    CHECK(refused_with(with32(entry + 4, 100000), "path"));
    const auto strings_size = static_cast<std::uint32_t>(data_file::index_bytes(s).size() - strings);
    CHECK(refused_with(with32(entry + 4, strings_size), "bad path")); // just past the last NUL
    CHECK(refused_with(with32(entry, 0x12345678u), "hash")); // hash of another path
    CHECK(refused_with(damaged(strings, 0), "path"));        // empty path
    CHECK(refused_with(damaged(strings + 7, '*'), "path"));  // invalid character
    IndexSpec up = sample();
    up.types[0].paths[0] = "../bat";
    CHECK(refused_with(data_file::index_bytes(up), "path"));
    IndexSpec zero = sample();
    zero.types[0].paths = {"akhnp9x"}; // FNV-1a 32 = 0
    CHECK(refused_with(data_file::index_bytes(zero), "hash"));
    IndexSpec longest = sample();
    longest.types[0].paths = {std::string(256, 'a')};
    CHECK(refused_with(data_file::index_bytes(longest), "path"));
}

NNH_TEST(index_validator_refuses_paths_out_of_order_or_repeated) {
    IndexSpec order = sample();
    order.types[0].paths = {"enemies/goblin", "enemies/bat"};
    CHECK(refused_with(data_file::index_bytes(order), "paths out of order"));
    IndexSpec twice = sample();
    twice.types[0].paths = {"enemies/bat", "enemies/bat"};
    CHECK(refused_with(data_file::index_bytes(twice), "paths out of order"));
    IndexSpec media = sample();
    media.media[0] = {"art/goblin", "art/bat"};
    CHECK(refused_with(data_file::index_bytes(media), "paths out of order"));
    IndexSpec across = sample(); // the same path in two types: data names are unique across types
    across.types[1].paths = {"enemies/bat"};
    CHECK(refused_with(data_file::index_bytes(across), "lookup"));
    IndexSpec groups = sample(); // the same path as a texture and a sound is fine
    groups.media[1] = {"art/bat"};
    const Bytes ok = data_file::index_bytes(groups);
    CHECK(validate_index(ok.data(), ok.size()).ok());
}

NNH_TEST(index_validator_refuses_a_bad_lookup) {
    const IndexSpec s = sample();
    const std::size_t lookup = data_file::index_lookup_at(s);
    Bytes file = data_file::index_bytes(s);
    data_file::put16(file, lookup, 7); // out of range (7 entries)
    data_file::seal_index(file);
    CHECK(refused_with(file, "lookup entry out of range"));
    Bytes same = data_file::index_bytes(s);
    data_file::put16(same, lookup + 2, same[lookup] | (same[lookup + 1] << 8)); // one entry twice
    data_file::seal_index(same);
    CHECK(refused_with(same, "lookup"));
    Bytes swapped = data_file::index_bytes(s); // data entries swapped: out of hash order
    const std::uint8_t a = swapped[lookup];
    swapped[lookup] = swapped[lookup + 2];
    swapped[lookup + 2] = a;
    data_file::seal_index(swapped);
    CHECK(refused_with(swapped, "lookup"));
}

NNH_TEST(index_validator_accepts_exactly_4096_data_assets) {
    IndexSpec s;
    s.types = {{"enemy", 1, {}}};
    for (int i = 0; i < fmt::kMaxDataEntries; ++i) {
        char path[16];
        std::snprintf(path, sizeof path, "e/%04d", i);
        s.types[0].paths.push_back(path);
    }
    const Bytes file = data_file::index_bytes(s);
    CHECK(validate_index(file.data(), file.size()).ok());
}

NNH_TEST(index_validator_refuses_sections_a_few_bytes_past_the_end) {
    Bytes file = data_file::index_bytes(sample());
    file.resize(data_file::index_strings_at(sample()) - 2); // the lookup cut short, no strings
    data_file::seal_index(file);
    CHECK(refused_with(file, "sections don't fit"));
}

NNH_TEST(index_validator_refuses_an_empty_type_key_and_types_that_stop_short) {
    IndexSpec empty = sample();
    empty.types[2].key = "";
    CHECK(refused_with(data_file::index_bytes(empty), "bad type key"));
    IndexSpec item = sample(); // item's one entry left out of every type
    Bytes file = data_file::index_bytes(item);
    const std::size_t second = data_file::index_types_at() + fmt::kTypeSize;
    data_file::put32(file, second + 24, 0);
    data_file::put32(file, second + fmt::kTypeSize + 20, 2);
    data_file::seal_index(file);
    CHECK(refused_with(file, "don't cover the data assets"));
}
