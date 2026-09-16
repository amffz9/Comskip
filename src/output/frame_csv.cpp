#include "frame_csv.h"
#include <cmath>
#include <iomanip>
#include <ios>
#include <locale>
#include <limits>
#include <ostream>
#include <stdexcept>

namespace comskip::output {
void validate_frame_csv(std::span<const frame_info> observations, FrameCsvOptions options) {
    if (!std::isfinite(options.frames_per_second) || options.frames_per_second <= 0)
        throw std::invalid_argument("CSV frame rate must be finite and positive");
    for (const auto& frame : observations) {
        const auto scene_change=static_cast<std::int64_t>(frame.schange_percent)*5;
        if (!std::isfinite(frame.ar_ratio) || !std::isfinite(frame.currentGoodEdge) ||
            !std::isfinite(frame.pts) || scene_change<std::numeric_limits<int>::min() ||
            scene_change>std::numeric_limits<int>::max())
            throw std::invalid_argument("Frame CSV observation is not representable");
    }
}
void write_frame_csv(std::ostream& destination, std::span<const frame_info> observations,
                     FrameCsvOptions options) {
    validate_frame_csv(observations,options);
    destination.imbue(std::locale::classic());
    destination << "sep=,\n"
        "frame,brightness,scene_change,logo,uniform,sound,minY,MaxY,ar_ratio,goodEdge,"
        "isblack,cutscene, MinX, MaxX, hasBright, Dimcount,PTS," << std::fixed
        << std::setprecision(6) << options.frames_per_second << '\n';
    for (std::size_t index=0; index<observations.size(); ++index) {
        const auto& frame=observations[index];
        destination << index+1 << ',' << frame.brightness << ','
            << static_cast<std::int64_t>(frame.schange_percent)*5 << ','
            << frame.logo_present << ',' << frame.uniform << ',' << frame.volume << ',' << frame.minY
            << ',' << frame.maxY << ',' << frame.ar_ratio << ',' << frame.currentGoodEdge << ','
            << frame.isblack << ',' << frame.cutscenematch << ',' << frame.minX << ',' << frame.maxX
            << ',' << frame.hasBright << ',' << frame.dimCount << ',' << frame.pts << ','
            << frame.cur_segment << ',' << frame.audio_channels << '\n';
    }
    if (!destination)
        throw std::ios_base::failure("Failed writing frame CSV output");
}
}
