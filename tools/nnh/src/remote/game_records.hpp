#pragma once
// The records a Development game writes into its log stream (spec 013, plan "Protocol"), decoded on the PC.
// The layouts are core's (engine/core/src/remote/remote_wire.hpp, whose encoders the tests use to build
// them). Pure; tested in tools/nnh/tests/test_game_stream.cpp.

#include <cstdint>
#include <string>
#include <vector>

#include "remote/remote_wire.hpp"

namespace nnh::tool::remote {

namespace wire = nnh::detail::remote;

/// One decoded record. Only the fields of its `type` are meaningful.
struct Record {
    wire::RecordType type = wire::RecordType::Tick;
    std::uint32_t frame = 0; ///< Hello: frame now; Tick: frame started; Screen/ScreenFailed: frame captured
    std::uint16_t version = 0;      ///< Hello
    std::uint16_t flags = 0;        ///< Hello
    std::uint32_t capabilities = 0; ///< Hello: bit (1 << nnh::Capability)
    std::string flavor;             ///< Hello: "Debug", "Development"
    std::uint16_t seq = 0;          ///< Late, Rejected, Dropped
    std::uint32_t wanted = 0;       ///< Late: the frame the message asked for
    std::uint32_t applied = 0;      ///< Late: the frame it was applied on
    wire::Reject reason = wire::Reject::None; ///< Rejected (any byte value is kept)
    wire::ScreenHeader screen;      ///< Screen (id, screen, layout, frame, size); ScreenFailed: id, screen
    std::vector<std::uint8_t> pixels; ///< Screen: everything after the fixed part, unchecked
};

/// Decodes a payload whose header decode_record_header() accepted (so its length suits its type).
/// Allocates (flavor, pixels). Never fails: values are checked by whoever uses them.
Record decode_record(const wire::RecordHeader& header, const std::uint8_t* payload);

} // namespace nnh::tool::remote
