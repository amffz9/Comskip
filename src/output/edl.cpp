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
        throw std::out_of_range("EDL frame offset exceeds the frame index range");
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
    if (!std::isfinite(time.count())) throw std::out_of_range("EDL timestamp exceeds the finite time range");
    std::array<char, 512> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), time.count(),
                                      std::chars_format::fixed, 2);
    if (result.ec != std::errc{}) throw std::out_of_range("EDL timestamp cannot be formatted");
    line.append(buffer.data(), result.ptr);
}
}

void write_edl(std::ostream& output, std::span<const CommercialInterval> intervals,
               const MediaDescription& media, const OutputOptions& options)
{
    if (!output) throw std::ios_base::failure("EDL output stream is not writable");
    if (!std::isfinite(media.frames_per_second) || media.frames_per_second <= 0)
        throw std::invalid_argument("EDL frame rate must be finite and positive");
    if (media.first_frame < 0 || media.timestamps.size() > static_cast<std::uint64_t>(std::numeric_limits<FrameIndex>::max() - media.first_frame))
        throw std::out_of_range("EDL timestamp span exceeds the frame index range");
    for (const auto time : media.timestamps)
        if (!std::isfinite(time.count())) throw std::invalid_argument("EDL timestamps must be finite");
    if (media.first_frame_timestamp && !std::isfinite(media.first_frame_timestamp->count()))
        throw std::invalid_argument("EDL first-frame timestamp must be finite");

    const auto correction = options.variant == EdlVariant::plus || options.mencoder_pts_correction
        ? media.first_frame_timestamp.value_or(timestamp(1, media)) : Seconds{};
    std::string text;
    for (const auto interval : intervals) {
        if (interval.start_frame < 0 || interval.end_frame < interval.start_frame)
            throw std::invalid_argument("EDL interval must have ordered, nonnegative frame indices");
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
    if (!output) throw std::ios_base::failure("Failed writing EDL output");
}
}
