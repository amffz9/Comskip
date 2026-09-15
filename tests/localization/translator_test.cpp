#include "translator.h"
#include <gtest/gtest.h>
#include <filesystem>

using comskip::config::Ini;
using comskip::localization::Translator;
TEST(Translator, LoadsCommittedCatalogsAndFormatsArguments) {
    const Translator english;
    const Translator spanish("es");
    EXPECT_EQ(english.format("using_settings", "recording.ini"), "Using recording.ini for settings.\n");
    EXPECT_EQ(spanish.format("using_settings", "recording.ini"), "Usando recording.ini para la configuración.\n");
    EXPECT_NE(std::string(english.text("usage")), spanish.text("usage"));
}
TEST(Translator, FallsBackToEnglishForMissingTranslation) {
    const Translator translator("es", Ini("message=Hello\n"), Ini{});
    EXPECT_STREQ(translator.text("message"), "Hello");
    EXPECT_THROW(translator.text("unknown"), std::out_of_range);
}
TEST(Translator, RejectsUnsupportedLanguageAndUnsafeFormatting) {
    EXPECT_THROW(Translator("../es"), std::invalid_argument);
    EXPECT_THROW((Translator("es", Ini("message=\"Hello {}\""), Ini("message=Hola"))), std::invalid_argument);
    EXPECT_THROW((Translator("es", Ini("message=Hello"), Ini("message=\"{0:100000}\""))), std::invalid_argument);
    EXPECT_THROW((Translator("es", Ini("message=Hello"), Ini("other=Hola"))), std::invalid_argument);
}
TEST(Translator, CommandLineLanguageOverridesSettings) {
    char program[] = "comskip", option[] = "--language=es";
    char* argv[] = {program, option};
    EXPECT_EQ(Translator::from_arguments(2, argv).language(), "es");
}
TEST(Translator, RejectsEmptyAndMissingCommandLineLanguage) {
    char program[] = "comskip", empty[] = "--language=", missing[] = "--language";
    char* empty_arguments[] = {program, empty};
    char* missing_arguments[] = {program, missing};
    EXPECT_THROW(Translator::from_arguments(2, empty_arguments), std::invalid_argument);
    EXPECT_THROW(Translator::from_arguments(2, missing_arguments), std::invalid_argument);
}
TEST(Translator, LoadsEditableExternalCatalogs) {
    const Translator translator("es", std::filesystem::path(COMSKIP_SOURCE_DIR) / "config/locales");
    EXPECT_EQ(translator.format("using_settings", "settings.ini"), "Usando settings.ini para la configuración.\n");
    EXPECT_THROW((Translator("es", std::filesystem::path(COMSKIP_SOURCE_DIR) / "nonexistent-locales")), std::runtime_error);
}
