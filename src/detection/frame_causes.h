#pragma once

#include <initializer_list>

namespace comskip::detection {

enum class FrameCause : int {
    logo = 1 << 0,
    caption = 1 << 1,
    scene_change = 1 << 2,
    non_uniform = 1 << 3,
    black = 1 << 4,
    aspect_ratio = 1 << 5,
    silence = 1 << 18,
    forced = 1 << 27,
    cutscene = 1 << 28,
    resolution_change = 1 << 29,
};

// Score and explanation bits carried alongside the frame causes in block_info.
// Values are part of the historical cutlist diagnostic format.
enum class BlockCause : long {
    strict = 1L << 6, non_strict = 1L << 7, combined = 1L << 8,
    logo = 1L << 9, exceeds = 1L << 10, aspect_ratio = 1L << 11,
    scene_change = 1L << 12, history_1 = 1L << 13, history_2 = 1L << 14,
    history_3 = 1L << 15, history_4 = 1L << 16, history_5 = 1L << 17,
    silence = 1L << 18, bright = 1L << 19, not_bright = 1L << 20,
    dim = 1L << 21, history_6 = 1L << 22, above_brightness = 1L << 23,
    above_uniformity = 1L << 24, above_length = 1L << 25,
    above_scene_change = 1L << 26, forced = 1L << 27,
    cutscene = 1L << 28, resolution_change = 1L << 29,
    history_7 = 1L << 29, history_8 = 1L << 30,
};

[[nodiscard]] constexpr long cause_value(BlockCause cause) noexcept {
    return static_cast<long>(cause);
}

[[nodiscard]] constexpr int cause_value(FrameCause cause) noexcept {
    return static_cast<int>(cause);
}

[[nodiscard]] constexpr long frame_cause_mask(std::initializer_list<FrameCause> causes) noexcept {
    long mask = 0;
    for (const auto cause : causes) {
        mask |= cause_value(cause);
    }
    return mask;
}

[[nodiscard]] constexpr long cut_cause(const long cause) noexcept {
    return cause & frame_cause_mask({
        FrameCause::caption, FrameCause::logo, FrameCause::scene_change,
        FrameCause::aspect_ratio, FrameCause::non_uniform, FrameCause::black,
        FrameCause::cutscene, FrameCause::resolution_change,
    });
}

} // namespace comskip::detection

struct RecordingContext;

char* CauseString(RecordingContext& context, int cause);
