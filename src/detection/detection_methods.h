#pragma once

namespace comskip::detection {

// Bit flags stored in RecordingSettings::commDetectMethod.
enum class DetectionMethod : int {
    black_frame = 1 << 0,
    logo = 1 << 1,
    scene_change = 1 << 2,
    resolution_change = 1 << 3,
    captions = 1 << 4,
    aspect_ratio = 1 << 5,
    silence = 1 << 6,
    cutscene = 1 << 7,
};

[[nodiscard]] constexpr bool method_enabled(int configured_methods, DetectionMethod method) noexcept {
    return (configured_methods & static_cast<int>(method)) != 0;
}

enum class CaptionType : int {
    none = 0,
};

} // namespace comskip::detection
