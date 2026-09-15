#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <span>

namespace comskip::output {
using Seconds = std::chrono::duration<double>;
using FrameIndex = std::int64_t;

struct CommercialInterval {
    FrameIndex start_frame{};
    FrameIndex end_frame{};
};

struct MediaDescription {
    double frames_per_second{};
    std::filesystem::path filename;
    // Contiguous presentation timestamps; the first entry describes first_frame.
    // An empty span selects constant-frame-rate timing. With timestamps, indices
    // outside the span clamp to its boundaries, as the detector historically did.
    std::span<const Seconds> timestamps;
    FrameIndex first_frame{1};
    // MEncoder's historical correction uses the first frame's presentation time,
    // which may differ from a frame duration when the source starts after zero.
    std::optional<Seconds> first_frame_timestamp;
};

enum class EdlVariant { standard, plus };

struct OutputOptions {
    FrameIndex frame_offset{};
    int skip_field{};
    bool mencoder_pts_correction{};
    EdlVariant variant{EdlVariant::standard};
};

// Writes headerless, tab-separated EDL with two decimal places and '\n' endings.
// Intervals of at most two frames are omitted; starts below frame five snap to
// zero. Plus EDL ignores frame_offset and always adds the first-frame timestamp.
// Throws invalid_argument/out_of_range for invalid input and ios_base::failure
// for a failed stream. It neither changes the stream's locale nor formatting.
void write_edl(std::ostream& output, std::span<const CommercialInterval> intervals,
               const MediaDescription& media, const OutputOptions& options = {});
}
