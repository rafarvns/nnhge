#pragma once
// A batch on the wire (spec 031 D54, FR-4, FR-18): ReloadBegin, then per file ReloadFile + its path (+ its CRC-32 for a
// put) and the put's bytes in ReloadChunk messages of at most 16 KB, as one byte string; the frame-stamped ReloadApply
// apart (it waits for the dry run). The game's limits are checked here first: more than 256 files, a file past 16 MiB
// or a batch past 32 MiB is refused locally ("too many changes for one reload: restart"). Pacing — no more than the game
// can have read (plan "Sending") — is the controller's. Tests: tools/nnh/tests/test_batch_sender.cpp.

#include <cstdint>
#include <string>
#include <vector>

#include "remote/remote_wire.hpp"

namespace nnh::tool::reload {

struct BatchFileData {
    std::string path;               ///< asset path
    bool removed = false;
    std::vector<std::uint8_t> bytes; ///< a put's content
};

struct EncodedBatch {
    std::vector<std::uint8_t> bytes; ///< every transfer message and payload, in order
    std::uint16_t files = 0;
    std::uint32_t total = 0;         ///< the Begin's total (paths + data)
    std::string error;               ///< non-empty: refused locally, nothing to send
};

/// Encodes batch `batch` (≠ 0). Paths must be valid asset paths (the build's own outputs).
EncodedBatch encode_batch(std::uint16_t batch, const std::vector<BatchFileData>& files);

/// The 16 bytes of ReloadApply for `batch` at `frame` (0 = the next frame), with `seq`.
std::vector<std::uint8_t> encode_apply(std::uint16_t batch, std::uint16_t files, std::uint32_t frame, std::uint16_t seq);
/// The 16 bytes of ReloadCancel.
std::vector<std::uint8_t> encode_cancel(std::uint16_t batch);
/// ReloadNotice + its path (the source, relative to the game's folder; cut to 255 bytes).
std::vector<std::uint8_t> encode_notice(bool needs_restart, const std::string& source, std::uint16_t others);

} // namespace nnh::tool::reload
