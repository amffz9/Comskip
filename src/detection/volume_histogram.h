#pragma once
#include <cstddef>
#include <expected>

namespace comskip::detection {
enum class VolumeBucketError { invalid_width, negative_volume, outside_histogram };

[[nodiscard]] constexpr std::expected<std::size_t,VolumeBucketError>
volume_histogram_bucket(int volume, std::size_t bucket_count, int bucket_width=10) noexcept {
    if (bucket_width<=0) return std::unexpected(VolumeBucketError::invalid_width);
    if (volume<0) return std::unexpected(VolumeBucketError::negative_volume);
    const auto bucket=static_cast<std::size_t>(volume/bucket_width);
    if (bucket>=bucket_count) return std::unexpected(VolumeBucketError::outside_histogram);
    return bucket;
}
}
