#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace comskip::detection {
struct LogoShrink {
    int head;
    int tail;
    double head_frames;
    double tail_frames;
};
inline double checked_logo_shrink_frames(double seconds, double fps) {
    const double frames = seconds * fps;
    if (!std::isfinite(seconds) || seconds < 0 || !std::isfinite(fps) || fps <= 0 ||
        !std::isfinite(frames) || frames < 0 ||
        frames >= static_cast<double>(std::numeric_limits<int>::max()) + 1.0)
        throw std::invalid_argument("Logo shrink must fit a nonnegative frame offset");
    return frames;
}
inline LogoShrink logo_shrink(double head, double tail, double fps) {
    const auto first = checked_logo_shrink_frames(head, fps);
    const auto last = checked_logo_shrink_frames(tail, fps);
    return {static_cast<int>(first), static_cast<int>(last), first, last};
}
inline int checked_logo_index(std::int64_t frame) {
    if (frame < std::numeric_limits<int>::min() || frame > std::numeric_limits<int>::max())
        throw std::out_of_range("Logo shrink arithmetic exceeds the frame index type");
    return static_cast<int>(frame);
}
struct ClosedLogoBlock { int start; int end; int frames_with_logo; bool retained; };
inline int add_logo_frames(int count, std::int64_t added) {
    return checked_logo_index(static_cast<std::int64_t>(count) + added);
}
struct StartedLogoBlock { int start; int frames_with_logo; };
inline StartedLogoBlock start_logo_block(int frame, int sample, int minimum_hits, int count) {
    if (sample <= 0 || minimum_hits <= 0)
        throw std::invalid_argument("Logo sampling and trend lengths must be positive");
    const std::int64_t history = static_cast<std::int64_t>(sample) * (minimum_hits - 1LL);
    return {checked_logo_index(std::max<std::int64_t>(static_cast<std::int64_t>(frame) - history, 0)),
            add_logo_frames(count, history)};
}
inline ClosedLogoBlock close_logo_block(int start, int frame, int sample, int frames_with_logo,
                                        const LogoShrink& shrink) {
    const std::int64_t end = static_cast<std::int64_t>(frame) - sample;
    const std::int64_t duration = end - start;
    // Preserve the original fractional tail comparison; applied offsets truncate.
    if (static_cast<long double>(duration) <= 2LL * shrink.head + shrink.tail_frames)
        return {-1, checked_logo_index(end), frames_with_logo, false};
    const std::int64_t removed = 2LL * sample + 2LL * shrink.head + shrink.tail;
    return {checked_logo_index(static_cast<std::int64_t>(start) + shrink.head),
            checked_logo_index(end - shrink.head - shrink.tail),
            checked_logo_index(static_cast<std::int64_t>(frames_with_logo) - removed), true};
}
struct LogoScanWindow { int begin; int end; };
inline LogoScanWindow logo_scan_window(int frame, int last, std::size_t storage, double radius) {
    if (!std::isfinite(radius) || radius < 0 ||
        radius >= static_cast<double>(std::numeric_limits<int>::max()) + 1.0)
        throw std::invalid_argument("Logo scan radius must fit a nonnegative frame offset");
    const auto limit = std::min<std::size_t>(storage,
        static_cast<std::size_t>(std::max(last, 0)));
    const double begin = std::max(1.0, static_cast<double>(frame) - radius);
    const double end = std::min(static_cast<double>(limit), static_cast<double>(frame) + radius);
    return {checked_logo_index(static_cast<std::int64_t>(begin)),
            checked_logo_index(static_cast<std::int64_t>(std::ceil(end)))};
}
}
