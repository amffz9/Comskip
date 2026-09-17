#pragma once

struct RecordingContext;

double get_frame_pts(RecordingContext& context, int frame);

namespace comskip::detection {

[[nodiscard]] inline double frame_duration(RecordingContext& context, int end_frame,
                                           int start_frame) {
    return get_frame_pts(context, end_frame) - get_frame_pts(context, start_frame);
}

} // namespace comskip::detection
