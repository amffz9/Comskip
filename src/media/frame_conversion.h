#pragma once

struct AVFrame;
struct SwsContext;

namespace comskip::media {
// Replaces the frame's buffers while retaining the caller-owned AVFrame object.
// On failure the input frame remains intact.
int convert_frame_to_8bit(AVFrame* frame, SwsContext*& context);
}
