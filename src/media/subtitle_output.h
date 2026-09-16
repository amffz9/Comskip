#pragma once
#include "media/caption_decoder.h"
#include <filesystem>
#include <string_view>

namespace comskip::media {
enum class SubtitleFormat { srt, sami };
// Per-destination subtitle encoder/writer. ASS regions/header stay owned by
// the caller; encoding copies their values and preserves supported styles.
// SRT uses FFmpeg's encoder and muxer. SAMI wraps FFmpeg's styled text using
// pugixml because FFmpeg supplies a SAMI reader, but no SAMI writer.
class SubtitleOutput {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    SubtitleOutput(const std::filesystem::path& destination, SubtitleFormat format, std::string_view ass_header);
    ~SubtitleOutput();
    SubtitleOutput(SubtitleOutput&&) noexcept;
    SubtitleOutput& operator=(SubtitleOutput&&) noexcept;
    SubtitleOutput(const SubtitleOutput&) = delete;
    SubtitleOutput& operator=(const SubtitleOutput&) = delete;
    void write(const CaptionCue& cue);
    // Idempotent completion; later write requires reset. Reports I/O failures.
    void finish();
    // Completes the previous file and starts an independently numbered file.
    void reset(const std::filesystem::path& destination);
};
}
