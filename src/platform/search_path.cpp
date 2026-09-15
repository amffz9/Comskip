#include "search_path.h"
#include <ranges>
#include <string>
#include <system_error>

namespace comskip::platform {
namespace {
std::filesystem::path utf8_path(std::string_view value)
{
    return std::filesystem::path(std::u8string_view(
        reinterpret_cast<const char8_t*>(value.data()), value.size()));
}
}
std::optional<std::filesystem::path> find_in_search_path(std::string_view filename,
    std::string_view search_path, const std::filesystem::path& working_directory,
    char separator)
{
    if (filename.empty()) return {};
    const auto name = utf8_path(filename);
    const auto probe = [&](const std::filesystem::path& directory)
        -> std::optional<std::filesystem::path> {
        const auto candidate = (directory.is_absolute() ? directory : working_directory / directory) / name;
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error)) return candidate.lexically_normal();
        return {};
    };
    if (auto found = probe(working_directory)) return found;
    for (const auto entry : search_path | std::views::split(separator)) {
        std::string_view directory(entry.begin(), entry.end());
        if (directory.size() >= 2 && directory.front() == '"' && directory.back() == '"') {
            directory.remove_prefix(1);
            directory.remove_suffix(1);
        }
        if (directory.empty()) continue;
        if (auto found = probe(utf8_path(directory))) return found;
    }
    return {};
}
}
