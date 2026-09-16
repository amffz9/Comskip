#include "input/caption_packet.h"

#include <array>
#include <charconv>
#include <string_view>
#include <utility>

namespace comskip::input {
namespace {

std::string_view trim_ascii(std::string_view text) {
    constexpr std::string_view whitespace = " \t\r\n\f\v";
    const auto first = text.find_first_not_of(whitespace);
    if (first == std::string_view::npos) return {};
    return text.substr(first, text.find_last_not_of(whitespace) - first + 1);
}

std::optional<int> decimal(std::string_view text) {
    text = trim_ascii(text);
    if (text.empty()) return std::nullopt;
    int value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) return std::nullopt;
    return value;
}

template <std::size_t Size>
std::expected<bool, CaptionPacketError> read_exact(std::istream& source,
    std::array<char, Size>& destination, CaptionPacketError truncated) {
    source.read(destination.data(), static_cast<std::streamsize>(destination.size()));
    if (source.bad()) return std::unexpected(CaptionPacketError::read_failure);
    const auto count = source.gcount();
    if (count == 0 && source.eof()) return false;
    if (count != static_cast<std::streamsize>(destination.size())) return std::unexpected(truncated);
    return true;
}

}

std::expected<std::optional<PersistedCaptionPacket>, CaptionPacketError>
read_persisted_caption_packet(std::istream& source, std::size_t maximum_payload) {
    std::array<char, 8> header{};
    const auto has_header = read_exact(source, header, CaptionPacketError::truncated_frame_header);
    if (!has_header) return std::unexpected(has_header.error());
    if (!*has_header) return std::optional<PersistedCaptionPacket>{};
    if (header.back() != ':') return std::unexpected(CaptionPacketError::invalid_frame_header);
    const auto frame = decimal(std::string_view(header.data(), 7));
    if (!frame || *frame < 0) return std::unexpected(CaptionPacketError::invalid_frame_header);

    std::array<char, 4> length_text{};
    const auto has_length = read_exact(source, length_text, CaptionPacketError::truncated_packet_length);
    if (!has_length) return std::unexpected(has_length.error());
    if (!*has_length) return std::unexpected(CaptionPacketError::truncated_packet_length);
    const auto length = decimal(std::string_view(length_text.data(), length_text.size()));
    if (!length || *length < 0 || static_cast<std::size_t>(*length) > maximum_payload)
        return std::unexpected(CaptionPacketError::invalid_packet_length);

    PersistedCaptionPacket packet{.frame = *frame};
    packet.payload.resize(static_cast<std::size_t>(*length));
    if (!packet.payload.empty()) {
        source.read(reinterpret_cast<char*>(packet.payload.data()),
                    static_cast<std::streamsize>(packet.payload.size()));
        if (source.bad()) return std::unexpected(CaptionPacketError::read_failure);
        if (source.gcount() != static_cast<std::streamsize>(packet.payload.size()))
            return std::unexpected(CaptionPacketError::truncated_packet);
    }
    return std::optional{std::move(packet)};
}

}
