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
TEST(Translator, LocalizesMediaStartupFailuresAndResults) {
    const Translator english;
    const Translator spanish("es");
    EXPECT_EQ(english.format("media_version", "Comskip 0.83.1"), "Comskip 0.83.1, made using ffmpeg\n");
    EXPECT_EQ(spanish.format("media_open_failed", "録画.ts"), "録画.ts: no se puede abrir el archivo\n");
    EXPECT_EQ(spanish.format("media_using_codec", "h264_qsv", "h264"), "Usando el códec h264_qsv en lugar de h264\n");
    EXPECT_STREQ(english.text("media_found_commercials"), "Commercials were found.\n");
    EXPECT_STREQ(spanish.text("media_found_commercials"), "Se encontraron anuncios.\n");
    EXPECT_EQ(spanish.format("media_seek_target", "   30.00"), "Buscar en    30.00\n");
    EXPECT_EQ(spanish.format("media_decoded_summary", 250, "10.00", "25.00"),
              "\n250 fotogramas decodificados en 10.00 segundos (25.00 fps)\n");
    EXPECT_EQ(spanish.format("media_decode_progress", "00:10", 250, "10.00", "25.00", "1.00", "25.00", 50),
              "00:10 - 250 fotogramas en 10.00 s(25.00 fps), 1.00 s(25.00 fps), 50%");
}
TEST(Translator, FormatsCliWarningsAndErrorsWithStableIdentifiers) {
    const Translator spanish("es");
    EXPECT_EQ(spanish.format("cli_invalid_option", "--unknown"), "Comskip: opción no válida \"--unknown\"\n");
    EXPECT_EQ(spanish.format("cli_invalid_argument", "--threads", "bad"),
              "Comskip: argumento no válido o ausente para --threads: \"bad\"\n");
    EXPECT_EQ(spanish.format("cli_throttle_schedule", "0600", "1200", "0900"),
              "\nComskip reduce la velocidad de 0600 a 1200.\nLa hora actual es 0900 ");
    EXPECT_TRUE(std::string(spanish.text("cli_read_ini_failed")).contains("INI"));
}

TEST(Translator, FormatsScoringDiagnosticsInEnglishAndSpanish) {
    const Translator english;
    const Translator spanish("es");
    EXPECT_EQ(english.format("scoring_score_before", "7", "1.25"),
              "Block 7 score:\tBefore - 1.25\t");
    EXPECT_EQ(spanish.format("scoring_score_before", "7", "1.25"),
              "Puntuación del bloque 7:\tAntes - 1.25\t");
    EXPECT_EQ(english.format("scoring_combined_strict_length", "2", "4", "30.00", "0.125000"),
              "Combining blocks 2 through 4 results in strict standard commercial length of 30.00 with a tolerance of 0.125000.\n");
    EXPECT_EQ(spanish.format("scoring_ar_differs", "3", "1.33", "1.78"),
              "La relación de aspecto del bloque 3 (1.33) difiere de la relación de aspecto dominante (1.78).\n");
}
