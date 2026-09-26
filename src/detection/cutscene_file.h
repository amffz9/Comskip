#pragma once

#include <cstdint>
#include <expected>
#include <iosfwd>
#include <span>
#include <vector>

namespace comskip::detection {
inline constexpr std::size_t maximum_cutscene_pixels = 400u * 300u;

// The configured cutscene frame is dumped once; zero disables dumping.
[[nodiscard]] constexpr bool records_cutscene_frame(int selected_frame, int frame) noexcept
{
    return selected_frame != 0 && frame == selected_frame;
}

struct CutsceneRecord {
    std::int32_t brightness{};
    std::vector<std::uint8_t> pixels;
    friend bool operator==(const CutsceneRecord&, const CutsceneRecord&) = default;
};

enum class CutsceneFileError { missing_header, missing_pixels, too_many_pixels, read_failed, write_failed };

std::expected<CutsceneRecord, CutsceneFileError> decode_cutscene(std::span<const std::uint8_t> bytes);
std::expected<std::vector<std::uint8_t>, CutsceneFileError> encode_cutscene(const CutsceneRecord& record);
std::expected<CutsceneRecord, CutsceneFileError> read_cutscene(std::istream& input);
std::expected<void, CutsceneFileError> write_cutscene(std::ostream& output, const CutsceneRecord& record);
}
