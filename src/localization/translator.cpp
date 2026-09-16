#include "translator.h"
#include "diagnostic.h"
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
    if (!input) throw diagnostics::DiagnosticError<std::runtime_error>(diagnostics::Code::catalog_open, {file.string()});
    std::string text{std::istreambuf_iterator<char>(input), {}};
    if (input.bad()) throw diagnostics::DiagnosticError<std::runtime_error>(diagnostics::Code::catalog_read, {file.string()});
    return config::Ini(text);
}
// Only positional replacement fields are accepted, so translators cannot change
// argument types or introduce format specifications that change output semantics.
std::size_t fields(std::string_view text) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '{') {
            if (++i == text.size()) throw diagnostics::DiagnosticError<std::invalid_argument>(diagnostics::Code::catalog_incomplete_field);
            if (text[i] == '{') continue;
            if (text[i] != '}') throw diagnostics::DiagnosticError<std::invalid_argument>(diagnostics::Code::catalog_field);
            ++count;
        } else if (text[i] == '}') {
            if (++i == text.size() || text[i] != '}')
                throw diagnostics::DiagnosticError<std::invalid_argument>(diagnostics::Code::catalog_brace);
        }
    }
    return count;
}
}
void Translator::validate_language(std::string_view language) {
    if (language != "en" && language != "es")
        throw diagnostics::DiagnosticError<std::invalid_argument>(diagnostics::Code::language, {std::string(language)});
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
            throw diagnostics::DiagnosticError<std::invalid_argument>(diagnostics::Code::catalog_mismatch, {id});
    }
    for (const auto& [id, message] : selected_.values())
        if (!english_.find(id)) throw diagnostics::DiagnosticError<std::invalid_argument>(diagnostics::Code::catalog_unknown, {id});
}
const char* Translator::text(std::string_view id) const {
    if (const auto* message = selected_.find(id)) return message->c_str();
    if (const auto* message = english_.find(id)) return message->c_str();
    throw std::out_of_range("Unknown message: " + std::string(id));
}
Translator Translator::fallback_from_arguments(int argc, char* const* argv) {
    std::string language="en";
    std::optional<std::string> override_language;
    std::filesystem::path ini_path="comskip.ini";
    for(int i=1;i<argc;++i) {
        const std::string_view argument(argv[i]);
        if(argument.starts_with("--language=")) override_language=argument.substr(11);
        else if(argument=="--language" && i+1<argc) override_language=argv[++i];
        else if(argument.starts_with("--ini=")) ini_path=utf8_path(argument.substr(6));
        else if(argument=="--ini" && i+1<argc) ini_path=utf8_path(argv[++i]);
    }
    try {
        std::ifstream input(ini_path,std::ios::binary);
        if(input) {
            const config::Ini settings(std::string{std::istreambuf_iterator<char>(input),{}});
            if(const auto* value=settings.find("language")) language=*value;
        }
    } catch(const std::exception&) { /* Invalid settings are reported by the real loader. */ }
    if(override_language) language=*override_language;
    return Translator(language=="es" ? "es" : "en");
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
            if (++i == argc) throw diagnostics::DiagnosticError<std::invalid_argument>(diagnostics::Code::option_value, {"--language"});
            override_language = argv[i];
        } else if (argument.starts_with("--ini=")) ini_path = utf8_path(argument.substr(6));
        else if (argument == "--ini") {
            if (++i == argc) throw diagnostics::DiagnosticError<std::invalid_argument>(diagnostics::Code::option_value, {"--ini"});
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
