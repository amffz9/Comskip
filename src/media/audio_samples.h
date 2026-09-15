#pragma once

#include <vector>

struct AVFrame;

namespace comskip::media {

// Each channel contains normalized floating-point samples, in the input order.
struct PlanarAudio {
    int sample_rate{};
    std::vector<std::vector<float>> channels;
};

// Converts sample representation only; preserves sample rate and channel layout.
// Throws std::invalid_argument for malformed frames, std::runtime_error on an
// FFmpeg conversion error, or std::bad_alloc on allocation failure.
PlanarAudio normalize_audio(const AVFrame& frame);

}
