#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <istream>
#include <optional>
#include <vector>

namespace comskip::input {

struct PersistedCaptionPacket {
    int frame{};
    std::vector<std::uint8_t> payload;
};

enum class CaptionPacketError {
    truncated_frame_header,
    invalid_frame_header,
    truncated_packet_length,
    invalid_packet_length,
    truncated_packet,
    read_failure,
};

// Reads one legacy `.data` record: a seven-column decimal frame plus ':', a
// four-column decimal payload size, and the payload. Clean EOF between records
// is represented by an empty optional.
[[nodiscard]] std::expected<std::optional<PersistedCaptionPacket>, CaptionPacketError>
read_persisted_caption_packet(std::istream& source, std::size_t maximum_payload);

}
