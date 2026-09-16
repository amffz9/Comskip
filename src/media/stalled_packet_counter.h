#pragma once
#include <cstddef>

namespace comskip::media {
// Report prolonged missing video progress once per consecutive packet window.
class StalledPacketCounter {
    std::size_t consecutive_{};
public:
    bool observe(bool video_advanced) noexcept {
        if (video_advanced) {
            consecutive_ = 0;
            return false;
        }
        if (++consecutive_ > 1000) {
            consecutive_ = 0;
            return true;
        }
        return false;
    }
};
}
