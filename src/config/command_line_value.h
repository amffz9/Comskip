#pragma once
#include <charconv>
#include <expected>
#include <string_view>

namespace comskip::config {
enum class PidParseError { invalid, out_of_range };

[[nodiscard]] inline std::expected<int,PidParseError> parse_transport_stream_pid(std::string_view text) noexcept {
    if (text.starts_with("0x") || text.starts_with("0X")) text.remove_prefix(2);
    if (text.empty()) return std::unexpected(PidParseError::invalid);
    unsigned int value{};
    const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),value,16);
    if (error == std::errc::result_out_of_range) return std::unexpected(PidParseError::out_of_range);
    if (error != std::errc{} || end != text.data()+text.size())
        return std::unexpected(PidParseError::invalid);
    if (value > 0x1fff) return std::unexpected(PidParseError::out_of_range);
    return static_cast<int>(value);
}
}
