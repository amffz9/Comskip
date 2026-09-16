#pragma once
#include "media/subtitle_output.h"
#include "media/subtitle_stream_decoder.h"
#include <optional>

namespace comskip::media {
struct CaptionOutputOptions {
    std::filesystem::path basename;
    bool srt{}, sami{};
    CaptionField field{CaptionField::first};
};
// Recording-owned lifecycle combining independent caption decoding/output.
class CaptionSession {
    CaptionOutputOptions options_;
    CaptionDecoder decoder_;
    std::unique_ptr<SubtitleStreamDecoder> stream_decoder_;
    std::vector<SubtitleOutput> outputs_;
    std::optional<CaptionTimestamp> last_time_;
    CaptionTimestamp stream_origin_{};
    std::vector<CaptionCue> stream_cues_;
    void finish_stream_cues();
    bool finished_{};
    void open_outputs();
    void write(std::span<const CaptionCue> cues);
public:
    explicit CaptionSession(CaptionOutputOptions options);
    ~CaptionSession();
    void consume(std::span<const std::uint8_t> a53, CaptionTimestamp timestamp);
    void consume_stored_packet(std::span<const std::uint8_t> packet, CaptionTimestamp timestamp);
    // Requested output prefers the selected standalone text stream; embedded
    // A53 remains available separately to the commercial/XDS detector.
    void select_stream(const AVCodecParameters& parameters, AVRational time_base);
    void consume_stream(std::span<const std::uint8_t> packet, std::int64_t pts,
                        std::int64_t duration, CaptionTimestamp recording_origin);
    void finish(CaptionTimestamp end);
    // Start a fresh analysis pass, replacing prior partial output and screens.
    void reset();
};
}
