#pragma once
#include <cstdint>
#include <string_view>

namespace comskip::media {

// Live mode follows a recording that is still being written when the input
// is a plain local file. FFmpeg's file protocol can wait for appended data;
// pipes and network protocols already block and are left unchanged.
[[nodiscard]] constexpr bool follows_growing_file(std::string_view filename) noexcept
{
    if (filename.empty()) return false;
    if (filename.starts_with("file:")) return true;
    const auto colon = filename.find(':');
    // A one-letter prefix is a Windows drive letter, not a protocol name.
    return colon == std::string_view::npos || colon == 1;
}

// How long a followed file may stop growing before the recording is treated
// as finished. Legacy live mode waited four seconds per retry.
[[nodiscard]] constexpr std::int64_t growing_file_timeout_us(int live_tv_retries) noexcept
{
    constexpr std::int64_t legacy_retry_wait_us = 4'000'000;
    return (live_tv_retries > 0 ? live_tv_retries : 1) * legacy_retry_wait_us;
}

}
