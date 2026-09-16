#pragma once
#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

namespace comskip::ui {
// Keep the original file first, then legacy MPEG-2 transport/ASF alternatives.
inline std::array<std::filesystem::path, 3> review_media_candidates(std::string_view utf8) {
    const auto original = std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
    auto transport = original; transport.replace_extension(".ts");
    auto asf = original; asf.replace_extension(".dvr-ms");
    return {original, std::move(transport), std::move(asf)};
}
}
