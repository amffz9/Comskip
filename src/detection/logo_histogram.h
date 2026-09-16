#pragma once

#include "detector_records.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace comskip::detection {

enum class LogoHistogramError {
    invalid_bucket_count,
    invalid_frame_count,
    invalid_edge_value,
    empty_samples,
    count_overflow
};

struct LogoHistogram {
    std::vector<std::uint64_t> counts;
    std::uint64_t denominator{};
    double quality{};
};

std::expected<double, LogoHistogramError> select_logo_quality(
    std::span<const std::uint64_t> counts, std::uint64_t denominator);

// Frame zero is the detector's sentinel. Observations are frames [1, frame_count).
// Validation completes before the result is populated, so failures are atomic.
std::expected<LogoHistogram, LogoHistogramError> build_logo_histogram(
    std::span<const frame_info> frames, std::size_t frame_count, std::size_t buckets);

}
