#include "translator.h"
#include "localization_catalogs.h"
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>

namespace comskip::localization {
namespace {
std::filesystem::path utf8_path(std::string_view value) {
    return std::filesystem::path(std::u8string_view(
        reinterpret_cast<const char8_t*>(value.data()), value.size()));
}
config::Ini read_catalog(const std::filesystem::path& file) {
    std::ifstream input(file, std::ios::binary);
    if (!input) throw std::runtime_error("Could not open message catalog: " + file.string());
    std::string text{std::istreambuf_iterator<char>(input), {}};
    if (input.bad()) throw std::runtime_error("Could not read message catalog: " + file.string());
    return config::Ini(text);
}
// Only positional replacement fields are accepted, so translators cannot change
// argument types or introduce format specifications that change output semantics.
std::size_t fields(std::string_view text) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '{') {
            if (++i == text.size()) throw std::invalid_argument("Incomplete catalog field");
            if (text[i] == '{') continue;
            if (text[i] != '}') throw std::invalid_argument("Catalog fields must be {}");
            ++count;
        } else if (text[i] == '}') {
            if (++i == text.size() || text[i] != '}')
                throw std::invalid_argument("Unmatched catalog brace");
        }
    }
    return count;
}
}
void Translator::validate_language(std::string_view language) {
    if (language != "en" && language != "es")
        throw std::invalid_argument("Unsupported language: " + std::string(language));
}
Translator::Translator(std::string_view language)
    : Translator(language, config::Ini(english_catalog),
                 config::Ini(language == "es" ? spanish_catalog : english_catalog)) {}
Translator::Translator(std::string_view language, const std::filesystem::path& catalog_directory)
    : Translator(language, config::Ini(english_catalog),
                 [&] {
                     validate_language(language);
                     return read_catalog(catalog_directory / (std::string(language) + ".ini"));
                 }()) {}
Translator::Translator(std::string_view language, const config::Ini& english,
                       const config::Ini& selected)
    : english_(english), selected_(selected), language_(language) {
    validate_language(language);
    for (const auto& [id, message] : english_.values()) {
        const auto expected = fields(message);
        if (const auto* translated = selected_.find(id); translated && fields(*translated) != expected)
            throw std::invalid_argument("Catalog argument mismatch: " + id);
    }
    for (const auto& [id, message] : selected_.values())
        if (!english_.find(id)) throw std::invalid_argument("Unknown catalog message: " + id);
}
const char* Translator::text(std::string_view id) const {
    if (const auto* message = selected_.find(id)) return message->c_str();
    if (const auto* message = english_.find(id)) return message->c_str();
    throw std::out_of_range("Unknown message: " + std::string(id));
}
Translator Translator::from_arguments(int argc, char* const* argv) {
    std::string language = *config::defaults().find("language");
    std::optional<std::string> override_language;
    std::filesystem::path ini_path;
    std::filesystem::path catalog_directory;
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument(argv[i]);
        if (argument.starts_with("--language=")) override_language = argument.substr(11);
        else if (argument == "--language") {
            if (++i == argc) throw std::invalid_argument("--language requires a value");
            override_language = argv[i];
        } else if (argument.starts_with("--ini=")) ini_path = utf8_path(argument.substr(6));
        else if (argument == "--ini") {
            if (++i == argc) throw std::invalid_argument("--ini requires a value");
            ini_path = utf8_path(argv[i]);
        }
    }
    if (ini_path.empty()) ini_path = "comskip.ini";
    std::ifstream input(ini_path, std::ios::binary);
    if (input) {
        try {
            const config::Ini settings(std::string{std::istreambuf_iterator<char>(input), {}});
            if (const auto* value = settings.find("language")) language = *value;
            if (const auto* value = settings.find("locale_directory"))
                catalog_directory = utf8_path(*value);
        } catch (const std::invalid_argument&) {
            // Preselection must not replace the real loader's localized error
            // report. A malformed document is still read and rejected there.
        }
    }
    if (override_language) language = std::move(*override_language);
    if (!catalog_directory.empty()) return Translator(language, catalog_directory);
    return Translator(language);
}
}
