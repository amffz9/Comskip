#include "../localization/diagnostic.h"
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
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::logo_sampling_interval_must_fit_a_positive_frame_index);
    // Sampling cannot be more frequent than once per decoded frame.
    return std::max(1, static_cast<int>(frames));
}
}
