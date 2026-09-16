#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace comskip::platform {
inline std::filesystem::path path_from_utf8(std::string_view bytes) {
    return std::filesystem::path(std::u8string(bytes.begin(), bytes.end()));
}
inline std::string path_to_utf8(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return {bytes.begin(), bytes.end()};
}
}
