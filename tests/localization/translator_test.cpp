#include "translator.h"
#include "review_messages.h"
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
TEST(Translator, LocalizesReviewHelpWithStableBindings) {
    const Translator english;
    const Translator spanish("es");
    const auto en_help = comskip::localization::review_help(english);
    const auto es_help = comskip::localization::review_help(spanish);
    EXPECT_EQ(en_help.back(), nullptr);
    EXPECT_EQ(es_help.back(), nullptr);
    EXPECT_STREQ(es_help.front(), "Ayuda: pulse cualquier tecla para cerrar");
    EXPECT_NE(std::string(en_help.front()), es_help.front());
    EXPECT_TRUE(std::string(spanish.text("review_help_volume")).starts_with("F2"));
    EXPECT_TRUE(std::string(spanish.text("review_help_cutscene")).starts_with("c "));
    EXPECT_TRUE(std::string(spanish.text("review_help_uniformity")).contains("non_uniformity"));
}
TEST(Translator, FormatsReviewLabelsAndWarnings) {
    const Translator spanish("es");
    EXPECT_EQ(spanish.format("review_program_name", "Nature"), "Nombre del programa: Nature");
    EXPECT_EQ(spanish.format("review_program_duration", "01:02"), "Duración del programa: 01:02");
    EXPECT_EQ(spanish.format("review_thresholds", 500, 500, 19),
              "max_volume=500, non_uniformity=500, max_avg_brightness=19");
    EXPECT_EQ(spanish.format("review_volume_bin", 3, 25), "volumen[3] = 25");
    EXPECT_TRUE(std::string(spanish.text("review_seeking_warning")).starts_with("ADVERTENCIA"));
    EXPECT_TRUE(spanish.format("review_frame_block", "30.0", 0, "B", 10, "S", 1, "U", "1.78",
                               2, "30.00", "0.50", "0.95", "L").contains("Bloque #2 Duración=30.00s"));
}
