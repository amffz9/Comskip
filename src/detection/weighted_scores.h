#pragma once
#include <cstdint>
#include <expected>
#include <span>

namespace comskip::detection {
struct WeightedScore { double score; std::uint64_t frames; };
enum class ScoreError { empty, invalid_percentile, invalid_sample, overflow };
// Preserve the detector's floor(total_frames * percentile) boundary convention.
std::expected<double, ScoreError> weighted_score_threshold(
    std::span<const WeightedScore> samples, double percentile);
}
