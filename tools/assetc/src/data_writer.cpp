// The converted data asset around a body of values (data_writer.hpp).

#include "data_writer.hpp"

#include <cstring>

#include "core/crc32.hpp"
#include "data/data_format.hpp"
#include "data/data_validate.hpp"

namespace nnh::assetc {

namespace {

void put(std::vector<std::uint8_t>& file, std::size_t at, std::uint32_t v, int width) {
    for (int i = 0; i < width; ++i)
        file[at + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(v >> (8 * i));
}

} // namespace

bool write_data(const Schema& schema, const detail::TypeInfo& type, const std::vector<std::uint8_t>& body,
                std::vector<std::uint8_t>& file, std::string& problem) {
    namespace fmt = detail::datafmt;
    const std::size_t size = fmt::kHeaderSize + body.size();
    if (size > fmt::kMaxFileBytes) {
        problem = "the converted file would be " + std::to_string(size) + " bytes: at most 64 KiB";
        return false;
    }
    file.assign(fmt::kHeaderSize, 0);
    file.insert(file.end(), body.begin(), body.end());
    std::memcpy(file.data(), fmt::kMagic, 4);
    put(file, fmt::kVersionAt, fmt::kVersion, 2);
    put(file, fmt::kSizeAt, static_cast<std::uint32_t>(size), 4);
    put(file, fmt::kCrcAt, detail::crc32(body.data(), body.size()), 4);
    const std::uint32_t fingerprint = schema.fingerprint(type);
    put(file, fmt::kFingerprintAt, fingerprint, 4);
    const detail::DataCheck check =
        detail::validate_data(file.data(), size, schema.registry(), type, fingerprint);
    if (!check.ok()) {
        problem =
            std::string("internal error: the converted file fails the engine's check (") + check.reason + ")";
        return false;
    }
    return true;
}

} // namespace nnh::assetc
