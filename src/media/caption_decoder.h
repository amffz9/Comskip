#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace comskip::media {
using CaptionTimestamp = std::chrono::microseconds;
enum class CaptionField { first, second };
struct CaptionRegion {
    std::string text;
    // Complete FFmpeg ASS event, including position/color/font overrides.
    std::string ass;
};
struct CaptionCue {
    CaptionTimestamp start{}, end{};
    std::vector<CaptionRegion> regions;
};
// Independent EIA-608 display decoder. A53 input is not consumed/modified, so
// callers can independently feed its 608/XDS metadata to the detector parser.
// The last visible screen is buffered until replacement/erase or drain(end).
class CaptionDecoder {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    explicit CaptionDecoder(CaptionField field = CaptionField::first);
    ~CaptionDecoder();
    CaptionDecoder(CaptionDecoder&&) noexcept;
    CaptionDecoder& operator=(CaptionDecoder&&) noexcept;
    CaptionDecoder(const CaptionDecoder&) = delete;
    CaptionDecoder& operator=(const CaptionDecoder&) = delete;
    std::vector<CaptionCue> decode(std::span<const std::uint8_t> a53, CaptionTimestamp timestamp);
    // Idempotent EOF. Decoding after drain requires reset. Times must be
    // nonnegative and monotonic within a recording; zero-duration cues omitted.
    std::vector<CaptionCue> drain(CaptionTimestamp end);
    void reset();
    const std::string& ass_header() const;
};
}
