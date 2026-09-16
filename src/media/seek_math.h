#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>

namespace comskip::media {
enum class SeekMathError { invalid_size, invalid_duration, invalid_time, invalid_time_base, out_of_range };

inline std::expected<double, SeekMathError> recording_duration(long frame_count, double fps) noexcept {
    if (frame_count <= 0 || !std::isfinite(fps) || fps <= 0) return std::unexpected(SeekMathError::invalid_duration);
    const double duration=static_cast<double>(frame_count)/fps;
    if (!std::isfinite(duration) || duration <= 0) return std::unexpected(SeekMathError::invalid_duration);
    return duration;
}

inline std::expected<std::int64_t, SeekMathError> byte_seek_position(
    std::int64_t size, double duration, double target) noexcept {
    if (size < 0) return std::unexpected(SeekMathError::invalid_size);
    if (!std::isfinite(duration) || duration <= 0) return std::unexpected(SeekMathError::invalid_duration);
    if (!std::isfinite(target)) return std::unexpected(SeekMathError::invalid_time);
    const long double ratio=std::clamp(static_cast<long double>(target)/duration,0.0L,1.0L);
    const long double position=static_cast<long double>(size)*ratio;
    if (position < 0 || position >= std::ldexp(1.0L, 63))
        return std::unexpected(SeekMathError::out_of_range);
    return static_cast<std::int64_t>(position);
}

inline std::expected<std::int64_t, SeekMathError> timestamp_seek_position(
    double target, double initial_pts, int time_base_num, int time_base_den,
    std::optional<std::int64_t> start_time = {}) noexcept {
    if (!std::isfinite(target) || !std::isfinite(initial_pts)) return std::unexpected(SeekMathError::invalid_time);
    if (time_base_num <= 0 || time_base_den <= 0) return std::unexpected(SeekMathError::invalid_time_base);
    const long double seconds=std::max(0.0L,static_cast<long double>(target)+initial_pts);
    const long double ticks=seconds*time_base_den/time_base_num;
    if (!std::isfinite(ticks) || ticks >= std::ldexp(1.0L, 63))
        return std::unexpected(SeekMathError::out_of_range);
    const auto integral=static_cast<std::int64_t>(ticks);
    if (start_time && (*start_time > 0 && integral > std::numeric_limits<std::int64_t>::max()-*start_time ||
                       *start_time < 0 && integral < std::numeric_limits<std::int64_t>::min()-*start_time))
        return std::unexpected(SeekMathError::out_of_range);
    return integral + start_time.value_or(0);
}
}
