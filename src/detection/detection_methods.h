#pragma once

namespace comskip::detection {

// Bit flags stored in RecordingSettings::commDetectMethod.
enum class DetectionMethod : int {
    logo = 1 << 1,
    scene_change = 1 << 2,
};

[[nodiscard]] constexpr bool method_enabled(int configured_methods, DetectionMethod method) noexcept {
    return (configured_methods & static_cast<int>(method)) != 0;
}

enum class CaptionType : int {
    none = 0,
};

} // namespace comskip::detection
