// Decoding a structured save from memory (spec 027, D46): validate the whole payload, check the key and the
// version, then T{}, the fields by id, and the migration steps. Nothing is written to the value before every
// check passed. The report is the caller's (a Report is ~350 bytes: one copy on the stack, not one per
// frame).

#include "saves/save_apply.hpp"
#include "saves/save_header.hpp"
#include "saves/save_internal.hpp"
#include "saves/save_validate.hpp"

#include <cstring>
#include <new>

namespace nnh::detail {

Result<void> save_decode_payload(const SaveTypeInfo& type, const std::uint8_t* data, std::size_t size,
                                 void* value, save::Report& report) {
    auto parsed = parse_save_struct_header(data, size);
    if (!parsed) return parsed.error();
    const SaveStructHeader& h = parsed.value();
    const std::uint8_t* fields = data + h.size;
    const std::uint8_t* end = data + size;
    if (auto ok = save_validate_fields(fields, static_cast<std::size_t>(end - fields)); !ok) return ok;
    if (h.key_hash != type.key_hash || std::strcmp(h.key, type.key) != 0) {
        return Error::make(ErrorCode::InvalidArgument, "a '%s' save, not '%s'", h.key, type.key);
    }
    if (h.version > type.version) {
        return Error::make(ErrorCode::Unsupported, "saved by version %lu; this game reads up to %lu",
                           static_cast<unsigned long>(h.version), static_cast<unsigned long>(type.version));
    }
    ::new (&report) save::Report{}; // in place: no temporary
    report.version = h.version;
    type.reset(value);
    SaveApply apply;
    apply.report = &report;
    apply_fields(apply, type, static_cast<std::uint8_t*>(value), fields, end);
    if (type.role == SaveRole::Type && h.version < type.version) {
        const save::OldFields old(fields, static_cast<std::uint32_t>(end - fields));
        report.migrated = save_run_steps(type, value, h.version, old);
    }
    return {};
}

Result<save::Report> save_decode(const SaveTypeInfo& type, const void* data, std::size_t size, void* value) {
    Result<save::Report> result{save::Report{}}; // every path returns `result`: no copy of the Report
    if (data == nullptr) {
        result = Error::make(ErrorCode::InvalidArgument, "no data");
        return result;
    }
    if (auto ok =
            save_decode_payload(type, static_cast<const std::uint8_t*>(data), size, value, result.value());
        !ok) {
        result = ok.error();
    }
    return result;
}

} // namespace nnh::detail
