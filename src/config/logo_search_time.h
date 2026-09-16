#pragma once

#include "../localization/diagnostic.h"
#include <chrono>
#include <stdexcept>
#include <utility>

namespace comskip::config {
inline int adjusted_logo_search_seconds(int seconds, int additional_minutes) {
    if (additional_minutes <= 0) return seconds;
    const auto extra = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::duration<long long, std::ratio<60>>{additional_minutes}).count();
    const auto adjusted = seconds < extra ? seconds + extra : seconds;
    if (!std::in_range<int>(adjusted))
        throw diagnostics::DiagnosticError<std::out_of_range>(
            diagnostics::Code::integer_range, {"added_recording / give_up_logo_search"});
    return static_cast<int>(adjusted);
}
}
