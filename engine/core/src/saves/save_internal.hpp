#pragma once
// Functions shared by the structured-save sources (spec 027): the in-memory decode the file layer builds on,
// the migration runner, the report's warning line and the dump-on-store hook. Engine-internal.

#include "nnh/log.hpp"
#include "nnh/saves.hpp"

#include <cstddef>
#include <cstdint>

namespace nnh {
NNH_LOG_CHANNEL_EXTERN(saves); // defined in save_report.cpp
} // namespace nnh

namespace nnh::detail {

/// Validates the payload `data` (structured header + fields), checks its key and version against `type`, then
/// resets `value` to T{}, applies the fields and runs the steps. `value` is unchanged on every error:
/// Corrupted (structure), InvalidArgument (another key), Unsupported (a raw payload, a newer format or
/// version). Messages don't name a file.
Result<save::Report> save_decode_payload(const SaveTypeInfo& type, const std::uint8_t* data, std::size_t size,
                                         void* value);

/// Runs the steps from version `from` up to the type's current version on `value`. Returns the steps run. A
/// missing step (impossible for a type that compiled) asserts in Debug/Development and is skipped in Release.
int save_run_steps(const SaveTypeInfo& type, void* value, std::uint32_t from, const save::OldFields& old);

/// Logs the one warning line of a load that used the backup, dropped or adjusted (nothing otherwise).
void save_log_report(const char* path, const SaveTypeInfo& type, const save::Report& report,
                     const char* backup_reason);

/// Debug/Development: writes `<path>.txt` (the dump of the encoded save in `data`) when NNH_SAVE_DUMP asks
/// for it.
void save_dump_on_store(const SaveTypeInfo& type, const char* path, const std::uint8_t* data,
                        std::size_t size);

/// Test hook of NNH_SAVE_DUMP: -1 reads the environment (once), 0 off, 1 on.
void save_set_dump_override(int mode);

} // namespace nnh::detail
