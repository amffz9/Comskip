#pragma once

#include <cstdint>
#include <utility>

namespace comskip::media {

// Keep the most recent position when an FFmpeg input has no AVIOContext.
// Format contexts for custom and non-seekable inputs are valid without one.
template <typename FormatContext, typename Tell>
[[nodiscard]] std::int64_t input_position(const FormatContext* format_context,
                                          const std::int64_t previous_position,
                                          Tell&& tell)
{
    if (format_context == nullptr || format_context->pb == nullptr) {
        return previous_position;
    }
    return std::forward<Tell>(tell)(format_context->pb);
}

} // namespace comskip::media
