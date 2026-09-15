#pragma once
#include <filesystem>
#include <optional>
#include <string_view>

namespace comskip::platform {
// Search the supplied working directory, then PATH entries in order.
std::optional<std::filesystem::path> find_in_search_path(std::string_view filename,
    std::string_view search_path, const std::filesystem::path& working_directory,
    char separator);
}
