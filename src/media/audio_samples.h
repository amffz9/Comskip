#pragma once

#include "ffmpeg_resources.h"
#include <vector>

namespace comskip::media {

// Each channel contains normalized floating-point samples, in the input order.
struct PlanarAudio {
    int sample_rate{};
    std::vector<std::vector<float>> channels;
};

// Converts sample representation only; preserves sample rate and channel layout.
// Throws std::invalid_argument for malformed frames, std::runtime_error on an
// FFmpeg conversion error, or std::bad_alloc on allocation failure.
// Reuses one converter while the input format, rate and layout are unchanged.
class AudioNormalizer {
public:
    AudioNormalizer() = default;
    AudioNormalizer(const AudioNormalizer&) = delete;
    AudioNormalizer& operator=(const AudioNormalizer&) = delete;
    ~AudioNormalizer();
    PlanarAudio normalize(const AVFrame& frame);

private:
    ResamplerPtr converter_;
    int format_{-1};
    int sample_rate_{};
    AVChannelLayout layout_{};
};

// One-shot conversion with a converter that is not reused.
PlanarAudio normalize_audio(const AVFrame& frame);

}
