#pragma once
#include <chrono>
#include <cstdint>
#include <optional>
#include <cmath>
#include <format>
#include <string>

namespace comskip::media {
inline int decode_completion_percent(double seconds, double duration) noexcept {
    if (!std::isfinite(seconds) || seconds < 0 || !std::isfinite(duration) || duration <= 0) return 0;
    if (seconds >= duration) return 100;
    return static_cast<int>(seconds / duration * 100);
}
inline std::string decode_position(double seconds) {
    const auto total = std::isfinite(seconds) && seconds >= 0 && seconds < std::ldexp(1.0, 63)
        ? static_cast<std::int64_t>(seconds) : 0;
    return std::format("{:2}:{:02}:{:02}", total / 3600, total / 60 % 60, total % 60);
}
struct DecodeProgressSnapshot {
    std::uint64_t frames{};
    std::chrono::duration<double> elapsed{}, interval{};
    std::uint64_t interval_frames{};
    double average_fps() const noexcept { return elapsed.count() > 0 ? frames / elapsed.count() : 0; }
    double interval_fps() const noexcept { return interval.count() > 0 ? interval_frames / interval.count() : 0; }
};
class DecodeProgress {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    void reset() noexcept { *this = {}; }
    void observe_frame(TimePoint now = Clock::now()) noexcept {
        if (!start_) start_ = last_report_ = now;
        ++frames_;
    }
    DecodeProgressSnapshot snapshot(TimePoint now = Clock::now()) const noexcept {
        if (!start_) return {};
        return {frames_, now - *start_, now - last_report_, frames_ - last_frames_};
    }
    std::optional<DecodeProgressSnapshot> report(TimePoint now = Clock::now()) noexcept {
        const auto result = snapshot(now);
        if (result.interval < std::chrono::seconds(1)) return std::nullopt;
        last_report_ = now;
        last_frames_ = frames_;
        return result;
    }
private:
    std::optional<TimePoint> start_;
    TimePoint last_report_{};
    std::uint64_t frames_{}, last_frames_{};
};
}
