#pragma once
#include "caption_session.h"
#include "diagnostic.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace comskip::media {
inline CaptionTimestamp video_caption_timestamp(double seconds) {
    if (!std::isfinite(seconds) ||
        seconds >= static_cast<double>(std::numeric_limits<std::int64_t>::max()) / 1000000)
        throw diagnostics::DiagnosticError<std::invalid_argument>(diagnostics::Code::invalid_video_caption_timestamp);
    return std::chrono::duration_cast<CaptionTimestamp>(std::chrono::duration<double>(std::max(0.0, seconds)));
}
}
