#pragma once
#include "ffmpeg_resources.h"

namespace comskip::media {
// Replaces the frame's buffers while retaining the caller-owned AVFrame object.
// On failure the input frame remains intact.
int convert_frame_to_8bit(AVFrame* frame, ScalerPtr& context);
}
