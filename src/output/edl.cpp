#include "../localization/diagnostic.h"
#include "edl.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>

namespace comskip::output {
namespace {
FrameIndex shifted(FrameIndex frame, FrameIndex offset)
{
    if (offset >= 0) return frame < offset ? 0 : frame - offset;
    if (frame > std::numeric_limits<FrameIndex>::max() + offset)
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::edl_frame_offset_exceeds_the_frame_index_range);
    return frame - offset;
}

Seconds timestamp(FrameIndex frame, const MediaDescription& media)
{
    if (media.timestamps.empty()) return Seconds(static_cast<double>(frame) / media.frames_per_second);
    const auto last = media.first_frame + static_cast<FrameIndex>(media.timestamps.size() - 1);
    const auto index = std::clamp(frame, media.first_frame, last) - media.first_frame;
    return media.timestamps[static_cast<std::size_t>(index)];
}

void append_seconds(std::string& line, Seconds time)
{
    if (!std::isfinite(time.count())) throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::edl_timestamp_exceeds_the_finite_time_range);
    std::array<char, 512> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), time.count(),
                                      std::chars_format::fixed, 2);
    if (result.ec != std::errc{}) throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::edl_timestamp_cannot_be_formatted);
    line.append(buffer.data(), result.ptr);
}
}

void write_edl(std::ostream& output, std::span<const CommercialInterval> intervals,
               const MediaDescription& media, const OutputOptions& options)
{
    if (!output) throw comskip::diagnostics::DiagnosticError<std::ios_base::failure>(comskip::diagnostics::Code::edl_output_stream_is_not_writable);
    if (!std::isfinite(media.frames_per_second) || media.frames_per_second <= 0)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::edl_frame_rate_must_be_finite_and_positive);
    if (media.first_frame < 0 || media.timestamps.size() > static_cast<std::uint64_t>(std::numeric_limits<FrameIndex>::max() - media.first_frame))
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::edl_timestamp_span_exceeds_the_frame_index_range);
    for (const auto time : media.timestamps)
        if (!std::isfinite(time.count())) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::edl_timestamps_must_be_finite);
    if (media.first_frame_timestamp && !std::isfinite(media.first_frame_timestamp->count()))
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::edl_first_frame_timestamp_must_be_finite);

    const auto correction = options.variant == EdlVariant::plus || options.mencoder_pts_correction
        ? media.first_frame_timestamp.value_or(timestamp(1, media)) : Seconds{};
    std::string text;
    for (const auto interval : intervals) {
        if (interval.start_frame < 0 || interval.end_frame < interval.start_frame)
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::edl_interval_must_have_ordered_nonnegative_frame_indices);
        if (interval.end_frame - interval.start_frame <= 2) continue;
        auto start = interval.start_frame < 5 ? 0 : interval.start_frame;
        auto end = interval.end_frame;
        if (options.variant == EdlVariant::standard) {
            start = shifted(start, options.frame_offset);
            end = shifted(end, options.frame_offset);
        }
        append_seconds(text, timestamp(start, media) + correction);
        text += '\t';
        append_seconds(text, timestamp(end, media) + correction);
        text += '\t';
        text += std::to_string(options.skip_field);
        text += '\n';
    }
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output) throw comskip::diagnostics::DiagnosticError<std::ios_base::failure>(comskip::diagnostics::Code::failed_writing_edl_output);
}
}
