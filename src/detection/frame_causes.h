#pragma once

namespace comskip::detection {

enum class FrameCause : int {
    logo = 1 << 0,
    caption = 1 << 1,
    scene_change = 1 << 2,
    non_uniform = 1 << 3,
    black = 1 << 4,
    aspect_ratio = 1 << 5,
    silence = 1 << 18,
    cutscene = 1 << 28,
    resolution_change = 1 << 29,
};

[[nodiscard]] constexpr int cause_value(FrameCause cause) noexcept {
    return static_cast<int>(cause);
}

} // namespace comskip::detection

struct RecordingContext;

char* CauseString(RecordingContext& context, int cause);
