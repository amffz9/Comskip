#include "../localization/diagnostic.h"
#include "scene_sampling.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace comskip::detection {
void validate_scene_brightness(int maximum, int test) {
    if (maximum < 0 || maximum > 255 || test < 0 || test > 255)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::scene_brightness_thresholds_must_be_between_0_and_255);
}
SceneSamplingGeometry validate_scene_sampling(int visible_width, int height, int stride, int border) {
    if (visible_width <= 0 || height <= 0 || stride < visible_width || border < 0)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_scene_sampling_geometry);
    // Existing directional scanners use signed integer offsets and need at least
    // one inward sampling iteration. Subtraction avoids overflow from 2*border.
    if (border > (visible_width - 2) / 2 || border > (height - 2) / 2 ||
        visible_width < 2 || height < 2)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::scene_border_leaves_no_directional_samples);
    const auto pitch = static_cast<std::size_t>(stride);
    const auto rows = static_cast<std::size_t>(height);
    if (pitch > static_cast<std::size_t>(std::numeric_limits<int>::max()) / rows)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::scene_sampling_exceeds_integer_addressable_storage);
    const auto retained_pitch = pitch - 2 * static_cast<std::size_t>(border);
    const auto retained_rows = rows - 2 * static_cast<std::size_t>(border);
    // Keep the legacy /16 normalization, while allowing small retained regions.
    const auto divisor = std::max<std::size_t>(1, retained_pitch * retained_rows / 16);
    const int step = visible_width < 600 ? 1 : visible_width > 1800 ? 4 :
                     visible_width > 1200 ? 3 : 2;
    return {pitch * rows, divisor, step};
}
}
