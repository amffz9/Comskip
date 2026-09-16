#pragma once
#include "detection/detector_records.h"
#include <iosfwd>
#include <span>

namespace comskip::output {
struct FrameCsvOptions { double frames_per_second{}; };

// Observations are written as one-based frame rows in the legacy replay format.
// The caller supplies only real observations; sentinel storage is excluded.
void validate_frame_csv(std::span<const frame_info> observations, FrameCsvOptions options);
void write_frame_csv(std::ostream& destination, std::span<const frame_info> observations,
                     FrameCsvOptions options);
}
