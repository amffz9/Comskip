#include "weighted_scores.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace comskip::detection {
std::expected<double, ScoreError> weighted_score_threshold(
    std::span<const WeightedScore> samples, double percentile)
{
    if (!std::isfinite(percentile) || percentile < 0 || percentile > 1)
        return std::unexpected(ScoreError::invalid_percentile);
    if (samples.empty()) return std::unexpected(ScoreError::empty);
    constexpr auto limit = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    std::uint64_t total = 0;
    for (const auto& sample : samples) {
        if (!std::isfinite(sample.score) || sample.frames == 0)
            return std::unexpected(ScoreError::invalid_sample);
        if (sample.frames > limit - total) return std::unexpected(ScoreError::overflow);
        total += sample.frames;
    }
    std::vector<WeightedScore> sorted(samples.begin(), samples.end());
    std::ranges::sort(sorted, {}, &WeightedScore::score);
    const auto target = percentile == 1 ? total : static_cast<std::uint64_t>(
        static_cast<long double>(total) * percentile);
    std::uint64_t cumulative = 0;
    for (const auto& sample : sorted) {
        cumulative += sample.frames;
        if (cumulative >= target) return sample.score;
    }
    return sorted.back().score;
}
}
