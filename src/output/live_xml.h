#pragma once
#include "xml_cutlists.h"

namespace comskip::output {
// Live detection pads intervals before export, as the final cut list does;
// this serializer only converts the already padded frame ranges to seconds.
void write_live_dvrmstb(std::ostream&, std::span<const FrameInterval>, double frames_per_second);
}
