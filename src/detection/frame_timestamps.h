#pragma once

struct RecordingContext;

double get_frame_pts(RecordingContext& context, int frame);
double get_fps(RecordingContext& context);
void set_fps(RecordingContext& context, double frame_period);
void set_frame_volume(RecordingContext& context, unsigned int frame, int volume);

namespace comskip::detection {

[[nodiscard]] inline double frame_duration(RecordingContext& context, int end_frame,
                                           int start_frame) {
    return get_frame_pts(context, end_frame) - get_frame_pts(context, start_frame);
}

} // namespace comskip::detection
