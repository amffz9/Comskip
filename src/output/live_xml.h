#pragma once
#include "xml_cutlists.h"

namespace comskip::output {
// Live detection historically applies padding directly to frame indexes before
// converting them to seconds. Keep that policy explicit and separate from the
// final-export timestamp/retained-range policy.
void write_live_dvrmstb(std::ostream&, std::span<const FrameInterval>,
                       double frames_per_second, int padding_frames);
}
