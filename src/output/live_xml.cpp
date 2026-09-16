#include "../localization/diagnostic.h"
#include "live_xml.h"
#include <cmath>
#include <stdexcept>
#include <vector>

namespace comskip::output {
void write_live_dvrmstb(std::ostream& output, std::span<const FrameInterval> frames,
                       double frames_per_second, int padding_frames) {
    if (!std::isfinite(frames_per_second) || frames_per_second <= 0)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::live_dvrmstb_export_requires_a_positive_finite_frame_rate);
    std::vector<TimeInterval> times;
    times.reserve(frames.size());
    for (const auto& interval : frames) {
        if (interval.start < 0 || interval.end < interval.start)
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_live_dvrmstb_commercial_frame_interval);
        // Arithmetic in seconds avoids signed integer overflow at the padding
        // boundary. The XML serializer validates adjusted ranges before writing.
        times.push_back({Seconds{(static_cast<double>(interval.start) + padding_frames) / frames_per_second},
                         Seconds{(static_cast<double>(interval.end) - padding_frames) / frames_per_second}});
    }
    write_dvrmstb(output, times);
}
}
