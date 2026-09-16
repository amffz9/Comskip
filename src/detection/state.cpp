#include "recording_context.h"

double get_frame_pts(RecordingContext& context, int f) {
    if (context.state.frame.empty()) {
            return(f / context.settings.fps);
    }
    if (f < 1)
        f = 1;
    if (f > context.state.frame_count -1)
        f = context.state.frame_count -1;
    return(context.state.frame[f].pts);
}
