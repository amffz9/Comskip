#pragma once
#include "caption_decoder.h"
extern "C" {
#include <libavcodec/codec_par.h>
#include <libavutil/rational.h>
}

namespace comskip::media {
// Standalone text subtitle decoder. Parameters/extradata are copied; packet
// times are stream ticks and returned cue times are absolute microseconds.
// Overlapping and out-of-order text events are supported. Bitmap subtitles
// require OCR and are explicitly rejected instead of silently producing text.
class SubtitleStreamDecoder {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    SubtitleStreamDecoder(const AVCodecParameters& parameters, AVRational time_base);
    ~SubtitleStreamDecoder();
    SubtitleStreamDecoder(SubtitleStreamDecoder&&) noexcept;
    SubtitleStreamDecoder& operator=(SubtitleStreamDecoder&&) noexcept;
    SubtitleStreamDecoder(const SubtitleStreamDecoder&) = delete;
    SubtitleStreamDecoder& operator=(const SubtitleStreamDecoder&) = delete;
    std::vector<CaptionCue> decode(std::span<const std::uint8_t> payload,
                                   std::int64_t pts, std::int64_t duration);
    // Idempotent EOF; reset recreates the codec from owned parameters for seek
    // or reopen and permits decoding again. Stateless codecs need no flush.
    std::vector<CaptionCue> drain();
    void reset();
    const std::string& ass_header() const;
};
}
