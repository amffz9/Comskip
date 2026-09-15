#pragma once
#include "ini.h"
#include <filesystem>
#include <format>
#include <string>
#include <string_view>

namespace comskip::localization {
// An application-owned value: callers explicitly pass translations to the UI.
// Stable message IDs and INI keys are never translated.
class Translator {
    config::Ini english_;
    config::Ini selected_;
    std::string language_;
public:
    explicit Translator(std::string_view language = "en");
    Translator(std::string_view language, const std::filesystem::path& catalog_directory);
    Translator(std::string_view language, const config::Ini& english,
               const config::Ini& selected);
    static Translator from_arguments(int argc, char* const* argv);
    static void validate_language(std::string_view language);
    std::string_view language() const noexcept { return language_; }
    const char* text(std::string_view id) const;
    template<class... Args> std::string format(std::string_view id, Args&&... args) const {
        return std::vformat(text(id), std::make_format_args(args...));
    }
};
}
