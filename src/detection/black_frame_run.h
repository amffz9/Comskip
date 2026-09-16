#pragma once

#include "detector_records.h"

#include <cstddef>
#include <span>

namespace comskip::detection {

// Returns the final active observation in a contiguous run with the requested
// cause. The span defines the complete active range; reserved or sentinel
// storage beyond it is never observable here.
[[nodiscard]] constexpr std::size_t contiguous_black_frame_run_end(
    std::span<const black_frame_info> frames, std::size_t start, int cause) noexcept
{
    if (start >= frames.size()) return frames.size();

    auto end = start;
    while (end + 1 < frames.size()
           && (frames[end + 1].cause & cause) != 0
           && frames[end + 1].frame == frames[end].frame + 1) {
        ++end;
    }
    return end;
}

}
