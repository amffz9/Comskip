#pragma once

#include <cstddef>
#include <expected>

namespace comskip::detection {

enum class BrightnessIndexError {
    out_of_range,
};

[[nodiscard]] constexpr std::expected<std::size_t, BrightnessIndexError>
brightness_histogram_index(int brightness, std::size_t bucket_count) noexcept
{
    if (brightness < 0 || static_cast<std::size_t>(brightness) >= bucket_count) {
        return std::unexpected(BrightnessIndexError::out_of_range);
    }
    return static_cast<std::size_t>(brightness);
}

} // namespace comskip::detection
