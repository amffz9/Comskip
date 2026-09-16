#include "../localization/diagnostic.h"
#include "logo_geometry.h"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace comskip::detection {
namespace {
std::vector<int> scan_axis(int begin, int end, int step, int skip_at, int skip_to) {
    std::vector<int> result;
    for (std::int64_t value=begin; value<end; value=value==skip_at ? skip_to : value+step)
        result.push_back(static_cast<int>(value));
    return result;
}
}
LogoScanGeometry validate_logo_scan(int width, int height, int stride, LogoScanOptions options) {
    if (width <= 0 || height <= 0 || stride < width || options.edge_radius <= 0 ||
        options.edge_step <= 0 || options.border < 0)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_logo_scan_geometry_or_edge_settings);
    const std::int64_t expansion=4LL*options.edge_step;
    const std::int64_t inset=static_cast<std::int64_t>(options.edge_radius)+options.border+expansion;
    const std::int64_t halo=static_cast<std::int64_t>(options.edge_radius)+1;
    if (2*inset >= width || 2*inset >= height || 2*halo >= width || 2*halo >= height)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::logo_radius_border_and_step_leave_no_safe_image_samples);
    const auto size=static_cast<std::uint64_t>(stride)*static_cast<std::uint64_t>(height);
    if (size > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::logo_image_exceeds_integer_addressable_storage);
    const int margin=static_cast<int>(inset);
    const int minimum=static_cast<int>(halo);
    return {static_cast<std::size_t>(size),static_cast<int>(expansion),margin,
        minimum,width-minimum-1,minimum,height-minimum-1,
        scan_axis(options.at_side ? width/2 : margin,width-margin,options.edge_step,width/3,2*width/3),
        scan_axis(options.at_bottom ? height/2 : margin,options.subtitles ? height/2 : height-margin,
                  options.edge_step,height/3,2*height/3)};
}
LogoFilterHistory validate_logo_filter(int filter, int sample, int current_frame, std::size_t storage_size) {
    if (filter < 0 || sample <= 0 || current_frame < 0 ||
        static_cast<std::size_t>(current_frame) >= storage_size)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_logo_filter_settings_or_frame_history);
    const std::int64_t delay=static_cast<std::int64_t>(filter)*sample;
    if (delay > std::numeric_limits<int>::max()/2)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::logo_filter_history_exceeds_representable_frame_indices);
    const int delayed = delay > current_frame ? std::min(1,current_frame) :
                        current_frame-static_cast<int>(delay);
    return {sample,static_cast<int>(delay),delayed,std::max(0,current_frame-sample+1),
            static_cast<std::int64_t>(current_frame)>2*delay};
}
}
