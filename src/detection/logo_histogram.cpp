#include "logo_histogram.h"

#include <cmath>
#include <limits>

namespace comskip::detection {
namespace {
constexpr std::size_t maximum_bucket_count = 256;

std::uint64_t forty_percent_floor(std::uint64_t value) noexcept
{
    // floor(value * 2 / 5), arranged so the multiplication cannot overflow.
    return (value / 5) * 2 + ((value % 5) * 2) / 5;
}
}

std::expected<double, LogoHistogramError> select_logo_quality(
    std::span<const std::uint64_t> counts, std::uint64_t denominator)
{
    if (counts.size() < 2 || counts.size() > maximum_bucket_count)
        return std::unexpected(LogoHistogramError::invalid_bucket_count);
    if (denominator == 0) return std::unexpected(LogoHistogramError::invalid_frame_count);

    std::uint64_t total = 0;
    for (const auto count : counts) {
        if (count > std::numeric_limits<std::uint64_t>::max() - total)
            return std::unexpected(LogoHistogramError::count_overflow);
        total += count;
    }

    std::uint64_t counter = 0;
    std::size_t index = 0;
    const auto boundary = forty_percent_floor(denominator);
    for (; index < counts.size(); ++index) {
        counter += counts[index];
        if (counter > boundary) break;
    }
    if (index == counts.size()) return std::unexpected(LogoHistogramError::empty_samples);

    if (index < counts.size() / 2) {
        index = counts.size() * 3 / 4;
    } else {
        if (index >= 2 && counts[index - 2] < counts[index]) index -= 2;
        for (int adjustment = 0; adjustment < 3 && index >= 1; ++adjustment)
            if (counts[index - 1] < counts[index]) --index;
    }
    return (static_cast<double>(index) + 0.5) / static_cast<double>(counts.size());
}

std::expected<LogoHistogram, LogoHistogramError> build_logo_histogram(
    std::span<const frame_info> frames, std::size_t frame_count, std::size_t buckets)
{
    if (buckets < 2 || buckets > maximum_bucket_count)
        return std::unexpected(LogoHistogramError::invalid_bucket_count);
    if (frame_count == 0 || frame_count > frames.size())
        return std::unexpected(LogoHistogramError::invalid_frame_count);
    if (frame_count == 1) return std::unexpected(LogoHistogramError::empty_samples);

    for (std::size_t frame = 1; frame < frame_count; ++frame) {
        const auto edge = frames[frame].currentGoodEdge;
        if (!std::isfinite(edge) || edge < 0.0 || edge > 1.0)
            return std::unexpected(LogoHistogramError::invalid_edge_value);
    }

    LogoHistogram result{std::vector<std::uint64_t>(buckets), frame_count, 0.0};
    for (std::size_t frame = 1; frame < frame_count; ++frame) {
        const auto bucket = static_cast<std::size_t>(frames[frame].currentGoodEdge * (buckets - 1));
        ++result.counts[bucket];
    }
    const auto quality = select_logo_quality(result.counts, result.denominator);
    if (!quality) return std::unexpected(quality.error());
    result.quality = *quality;
    return result;
}

}
