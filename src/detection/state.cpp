#include "recording_context.h"
#include <algorithm>

double get_frame_pts(RecordingContext& context, int f) {
    if (context.state.frame.size() <= 1 || context.state.frame_count <= 1) {
            return(f / context.settings.fps);
    }
    const auto last = std::min(static_cast<std::size_t>(context.state.frame_count - 1),
                               context.state.frame.size() - 1);
    const auto index = std::clamp(static_cast<std::size_t>(std::max(f, 1)),
                                  std::size_t{1}, last);
    return context.state.frame[index].pts;
}
