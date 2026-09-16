#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace comskip::detection {
inline int logo_sampling_interval(double frames_per_second, double seconds) {
    const auto frames = frames_per_second * seconds;
    if (!std::isfinite(frames_per_second) || frames_per_second <= 0 ||
        !std::isfinite(seconds) || seconds <= 0 || !std::isfinite(frames) ||
        frames >= static_cast<double>(std::numeric_limits<int>::max()) + 1.0)
        throw std::invalid_argument("Logo sampling interval must fit a positive frame index");
    // Sampling cannot be more frequent than once per decoded frame.
    return std::max(1, static_cast<int>(frames));
}
}
