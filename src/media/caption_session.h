#pragma once
#include "media/subtitle_output.h"
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
    std::vector<SubtitleOutput> outputs_;
    std::optional<CaptionTimestamp> last_time_;
    bool finished_{};
    void open_outputs();
    void write(std::span<const CaptionCue> cues);
public:
    explicit CaptionSession(CaptionOutputOptions options);
    ~CaptionSession();
    void consume(std::span<const std::uint8_t> a53, CaptionTimestamp timestamp);
    void consume_stored_packet(std::span<const std::uint8_t> packet, CaptionTimestamp timestamp);
    void finish(CaptionTimestamp end);
    // Start a fresh analysis pass, replacing prior partial output and screens.
    void reset();
};
}
