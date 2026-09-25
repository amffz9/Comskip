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
TEST(Translator, FormatsDiagnosticsSearchAndThresholdMessages) {
    const Translator english;
    const Translator spanish("es");
    EXPECT_EQ(english.format("diagnostics_path_for", "comskip.ini", "C:/config/comskip.ini"),
              "Path for comskip.ini: C:/config/comskip.ini\n");
    EXPECT_EQ(spanish.format("diagnostics_path_missing", "comskip.dictionary"),
              "No se encontró comskip.dictionary\n");
    EXPECT_EQ(english.format("diagnostics_score_percentile", "95.00", 120, "0.75"),
              "The 95.00 percentile of 120 frames is 0.75\n");
    EXPECT_EQ(spanish.format("diagnostics_logo_histogram", "12.50000"),
              "Histograma de logotipos - 12.50000\n");
    EXPECT_EQ(english.format("diagnostics_show_histogram", "0.50000"),
              "Show Histogram - 0.50000\n");
    EXPECT_EQ(spanish.format("diagnostics_show_uniform", "0.25000"),
              "Mostrar uniformidad - 0.25000\n");
    EXPECT_EQ(english.format("diagnostics_show_histogram_title", "Brightness"),
              "Show Brightness Histogram\n");
    EXPECT_STREQ(spanish.text("diagnostics_sending_frame"), "Enviando fotograma al archivo\n");
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
TEST(Translator, FormatsCaptionDictionaryDiagnosticsInEnglishAndSpanish) {
    const Translator english;
    const Translator spanish("es");
    EXPECT_EQ(english.format("caption_dictionary_found", "OPEN NOW", "7"),
              "OPEN NOW found in cc_text_block 7\n");
    EXPECT_EQ(spanish.format("caption_dictionary_search", "OFERTA"),
              "Buscando: OFERTA\n");
    EXPECT_EQ(spanish.format("caption_dictionary_block_error", "12"),
              "Se produjo un error al buscar el cblock correcto para el cblock de texto de subtítulos 12.\n");
}
TEST(Translator, FormatsCaptionXdsDiagnosticsInEnglishAndSpanish) {
    const Translator english;
    const Translator spanish("es");
    EXPECT_EQ(english.format("caption_xds_program_start", "42", "09", "05", "7", "4"),
              "XDS[42]: Program Start Time 09:05 7/4\n");
    EXPECT_EQ(spanish.format("caption_xds_program_name", "42", "Noticias"),
              "XDS[42]: Nombre del programa: Noticias\n");
    EXPECT_EQ(spanish.format("caption_xds_vchip", "42", " 8", " f", "10", "20"),
              "XDS[42]: V-Chip:  8  f 10 20\n");
    EXPECT_EQ(english.format("caption_xds_bytes", "42", " 1  a ff"),
              "XDS[42]:  1  a ff ");
}
TEST(Translator, FormatsCaptionControlAndBlockDiagnostics) {
    const Translator english;
    const Translator spanish("es");
    EXPECT_EQ(english.format("caption_control_unknown", "    42", " A"),
              "\nFrame -     42 Control Code Found:\tUnknown code!! -  A\n");
    EXPECT_EQ(english.format("caption_control_end", "    42", "1", "0"),
              "Frame -     42 Control Code Found:\tEnd of Caption\tOn Screen - 1\tOff Screen - 0\n");
    EXPECT_EQ(spanish.format("caption_field_order", "1", "15"),
              "Orden de campos CC: 1. Parece haber 15 paquetes.\n");
    EXPECT_EQ(spanish.format("caption_block_summary", "    10", "    20", " 1", " 2", "POPON"),
              "Inicio -     10\tFin -     20\tCCF -  1\tCCL -  2\tTipo - POPON\n");
    EXPECT_STREQ(spanish.text("caption_type_popon"), "EMERGENTE");
    EXPECT_STREQ(english.text("caption_type_commercial"), "COMMERCIAL");
}
TEST(Translator, FormatsLogoDiagnosticsWithStableWidthsAndPercentages) {
    const Translator english;
    const Translator spanish("es");
    EXPECT_EQ(english.format("logo_block_row", "    10", "    20", "0:00:00", "1.2", "3.4"),
              "Logo start -     10\tend -     20\tlength - 0:00:00\tbefore:1.2 s\t after:3.4 s\n");
    EXPECT_EQ(english.format("logo_edge_too_big", "401", "12.50"),
              "Edge count - 401\tPercentage of screen - 12.50% TOO BIG, CAN'T BE A LOGO.\n");
    EXPECT_EQ(spanish.format("logo_found_bounds", "25", "1", "2", "3", "4"),
              "Logotipo encontrado en el fotograma 25\tlogoMinX=1\tlogoMaxX=2\tlogoMinY=3\tlogoMaxY=4\n");
    EXPECT_EQ(spanish.format("logo_mask_heading", spanish.text("logo_mask_diagonal_1")),
              "\nMáscara de logotipo diagonal 1 \n     ");
}
TEST(Translator, FormatsLogoSearchAndCutpointDiagnostics) {
    const Translator english;
    const Translator spanish("es");
    EXPECT_EQ(english.format("detection_logo_cut_disappears", "    42", "1.250"),
              "Frame     42 (1.250s) - Cutpoint added when Logo disappears\n");
    EXPECT_EQ(english.format("detection_logo_cut_after_disappears", "    42", "1.250", "3", "87"),
              "Frame     42 (1.250s) - Cutpoint added 3 seconds after Logo disappears at change percentage of 87\n");
    EXPECT_EQ(spanish.format("detection_logo_search_restart", "10", "250"),
              "\nNo se encontró el logotipo en los fotogramas 10 a 250; se reinicia la búsqueda.\n");
    EXPECT_EQ(spanish.format("detection_logo_cut_before_appears", "    42", "1.250", "2", "91"),
              "Fotograma     42 (1.250s) - Punto de corte añadido 2 segundos antes de aparecer el logotipo con un porcentaje de cambio de 91\n");
}
TEST(Translator, FormatsDetectionHistogramAndSilenceDiagnostics) {
    const Translator english;
    const Translator spanish("es");
    EXPECT_EQ(english.format("detection_aspect_histogram_row", " 1.78", "   250", "100.0"),
              "Aspect Ratio   1.78 found on    250 frames totalling \t100.0%\n");
    EXPECT_EQ(english.format("detection_audio_histogram_row", "  2", "    75", "60.0"),
              "Audio channels   2 found on     75 frames totalling \t60.0%\n");
    EXPECT_EQ(spanish.format("detection_volume_plateau", "20", "12", "30", "4"),
              "Meseta@[20] fotogramas 12, volumen 30, distancia 4 segundos\n");
    EXPECT_EQ(spanish.format("detection_long_silent_segment", "100", "149"),
              "\nSegmento silencioso largo detectado desde los fotogramas 100 hasta 149\n");
}
TEST(Translator, FormatsDetectionBlockReportsAndCompletion) {
    const Translator english;
    const Translator spanish("es");
    EXPECT_EQ(english.format("detection_ac_block_row", "2", "    10", "    20", " 2", "0:00:00"),
              "Block: 2\tStart:     10\tEnd:     20\taudio channels:  2\tLength: 0:00:00\n");
    EXPECT_EQ(english.format("detection_ar_block_row", "3", "    10", "    20", "1.78", "0:00:00",
                             "1920", "1080", "  0", "  1", "1919", "1079"),
              "Block: 3\tStart:     10\tEnd:     20\tAR_R: 1.78\tLength: 0:00:00, [1920x1080] minX=  0, minY=  1, maxX=1919, maxY=1079\n");
    EXPECT_EQ(spanish.format("detection_ar_join_same_ratio", "2", "3", "1.78"),
              "Se unen los bloques AR 2 y 3 porque ambos tienen una relación de aspecto de 1.78\n");
    EXPECT_EQ(spanish.format("detection_frames_processed", "250"),
              "\n250 fotogramas procesados\n");
}
TEST(Translator, FormatsCaptionTranscriptRows) {
    const Translator english;
    const Translator spanish("es");
    EXPECT_EQ(english.format("detection_caption_transcript_row", 2, "    10", "    20", "   5", "Hello"),
              "2) S:    10 E:    20 L:   5 Hello\n");
    EXPECT_EQ(spanish.format("detection_caption_transcript_row", 2, "    10", "    20", "   5", "Hola"),
              "2) S:    10 E:    20 L:   5 Hola\n");
}
TEST(Translator, FormatsCutlistThresholdAndHeuristicMessages) {
    const Translator english;
    const Translator spanish("es");
    EXPECT_EQ(english.format("cutlists_threshold_used", "0.8750"), "Threshold used - 0.8750\n");
    EXPECT_EQ(english.format("cutlists_h5_delete_after_last", 4, 12),
              "H5 Deleting cblock 4 of 12 seconds because it comes after the last commercial and its too short.\n");
    EXPECT_EQ(spanish.format("cutlists_threshold_rounded", "0.88"), "\tDespués del redondeo - 0.88\n");
    EXPECT_EQ(english.format("cutlists_keep_last_shorten", 2, 5),
              "Shortening commercial block 2 because the last 5 seconds should always be kept.\n");
    EXPECT_EQ(english.format("cutlists_total_commercial_length", "1:02"),
              "Total commercial length:    1:02\n");
    EXPECT_EQ(english.format("cutlists_h6_delete_length", 3),
              "H6 Deleting block 3 because it is part of a too short or too long commercial.\n");
    EXPECT_TRUE(std::string(spanish.text("cutlists_statistics")).contains("Proporción de logotipo"));
    EXPECT_STREQ(english.text("cutlists_initial_list"),
                 "\n\n\t---------------------\n\tInitial Commercial List\n\t---------------------\n");
    EXPECT_TRUE(std::string(spanish.text("cutlists_weighted_heading")).contains("Lista de bloques"));
    EXPECT_STREQ(spanish.text("cutlists_no_change"), "Sin cambios\n");
}
TEST(Translator, LocalizesRuntimeAllocationAndCsvLifecycleMessages) {
    const Translator english;
    const Translator spanish("es");
    EXPECT_STREQ(english.text("runtime_allocate_frame_array_failed"),
                 "Could not allocate memory for frame array\n");
    EXPECT_STREQ(spanish.text("runtime_allocate_audio_blocks_failed"),
                 "No se pudo asignar memoria para el array de bloques de canales de audio\n");
    EXPECT_STREQ(english.text("csv_loaded"), "CSV file loaded into memory.\n");
    EXPECT_STREQ(spanish.text("csv_close_window"),
                 "Cierre la ventana cuando termine\n");
}
TEST(Translator, FormatsAnalysisLifecycleDiagnosticsWithStableEnglishLayout) {
    const Translator english;
    const Translator spanish("es");
    EXPECT_EQ(english.format("analysis_retry_packet", 10, 20, 30, 40),
              "Retry t_pos=10, l_pos=20, t_pts=30, l_pts=40\n");
    EXPECT_EQ(english.format("analysis_retry", 2, 150, "   12.50"),
              "\nRetry=2 at frame=150, time=   12.50 seconds\n");
    EXPECT_EQ(english.format("analysis_parsed_frames", 250, 400, "   25.00"),
              "\nParsed 250 video frames and 400 audio frames at    25.00 fps\n");
    EXPECT_STREQ(english.text("analysis_selftest_seek_ok"),
                 "\nSelftest 1 OK: Seektest\n");
    EXPECT_EQ(spanish.format("analysis_selftest_failed", 3),
              "\nAutoprueba 3 FALLIDA\n");
    EXPECT_EQ(spanish.format("analysis_retry_target", 100, 200),
              "Posición objetivo del reintento=100, pts=200\n");
    EXPECT_EQ(spanish.format("analysis_maximum_volume", 32767),
              "\nEl volumen máximo encontrado es 32767\n");
}
TEST(Translator, FormatsBlockValidationAndThresholdDiagnostics) {
    const Translator english;
    const Translator spanish("es");
    EXPECT_EQ(english.format("blocks_negative_cutpoint_too_short",
                             english.text("blocks_reason_black_frame"), "    42"),
              "Negative Black Frame   cutpoint at     42, commercial too short\n");
    EXPECT_EQ(english.format("blocks_cut_distribution", english.text("blocks_reason_volume"),
                             "  7", "  2", "3.5000"),
              "Distribution of Volume        cutting:   7 positive and   2 negative, ratio is 3.5000\n");
    EXPECT_EQ(spanish.format("blocks_cut_confidence_too_low",
                             spanish.text("blocks_reason_scene_change"), "  1", "  8"),
              "Confianza del corte de Cambio de escena:   1 de   8 son estrictos, demasiado baja\n");
    EXPECT_EQ(spanish.format("blocks_setting_brightness_threshold", "19"),
              "Se establece el umbral de brillo en 19\n");
    EXPECT_EQ(spanish.format("blocks_single_missing_audio_frames", "3"),
              "Fotogramas aislados sin audio: 3\n");
}
TEST(Translator, FormatsBlackFrameBlockDiagnostics) {
    const Translator english;
    const Translator spanish("es");
    EXPECT_EQ(english.format("blocks_remove_loud_black_frame", 2, 42, 19, 15, 3, 4),
              "2 - Removing black frame 42, from black frame list because volume 19 is more than 15, brightness 3, uniform 4\n");
    EXPECT_EQ(english.format("blocks_black_frame_list_heading", 7),
              "Black Frame List\n---------------------------\nBlack Frame Count = 7\nnr \tframe\tpts\tbright\tuniform\tvolume\t\tcause\tdimcount  bright   type\n");
    EXPECT_EQ(english.format("blocks_create_cblock", 1, 10, 12, 30, 28, "black frame", 2, 2),
              "Creating cblock 1 From 10 (12) to 30 (28) because of black frame with 2 head and 2 tail\n");
    EXPECT_EQ(english.format("blocks_logo_quality", std::format("{:.5f}", 0.125)),
              "Set Logo Quality = 0.12500\n");
    EXPECT_EQ(spanish.format("blocks_combine_blocks", 2, 3, 120, 4),
              "Se combinan los bloques 2 y 3 en 120 porque solo los separan 4 fotogramas negros.\n");
    EXPECT_EQ(spanish.format("blocks_join_logo_blocks", 2, 3, 120),
              "Se unen los bloques 2 y 3 en el fotograma 120 porque ambos tienen un logotipo.\n");
}
TEST(Translator, FormatsDetectorStorageGrowthDiagnostics) {
    const Translator english;
    const Translator spanish("es");
    EXPECT_EQ(english.format("storage_resize_frame", 90000),
              "Resizing frame buffer to accommodate 90000 entries.\n");
    EXPECT_EQ(spanish.format("storage_resize_caption_text", 100),
              "Se cambia el tamaño del búfer de texto de subtítulos para alojar 100 entradas.\n");
}

TEST(Translator, FormatsLiveDetectionDiagnosticsWithStableEnglishLayout) {
    const Translator english;
    const Translator spanish("es");
    EXPECT_EQ(english.format("live_cutpoint_without_logo", 42, "Black Frame  "),
              "[42] Cutpoint Black Frame   without logo\n");
    EXPECT_EQ(english.format("live_logo_detected_setting_commercial", 100, 125, 80, 100),
              "Logo detected between frames 100 and 125.  Setting commercial to 80 to 100.\n");
    EXPECT_EQ(english.format("live_candidate_extended", 100, 850, "30.00", "29.00"),
              "--start: 100, end: 850, len: 30.00s\t29.00s\n");
    EXPECT_EQ(english.format("live_candidate_started", 100, 850, "30.00"),
              "\n  start: 100, end: 850, len: 30.00s\n");
    EXPECT_EQ(english.format("live_output_interval", 3, 100, 850),
              "Output: 3 - start: 100   end: 850\n");
    EXPECT_EQ(spanish.format("live_silence_and_dark", 42), "[42] Silencio y oscuridad\n");
    EXPECT_EQ(spanish.format("live_candidate_confirmed", 100, 850, 30),
              "\n  inicio: 100, fin: 850, duración: 30s\n");
}

TEST(Translator, FormatsVideoDecoderDiagnosticsWithStableEnglishLayout) {
    const Translator english;
    const Translator spanish("es");
    EXPECT_EQ(english.format("media_format_changed", 1920, 1080), "Format changed to [1920 : 1080]\n");
    EXPECT_EQ(english.format("media_initial_video_pts", "    12.500"), "\nInitial video pts =     12.500\n");
    EXPECT_EQ(english.format("media_framerate_forced", "29.970", 42),
              "Framerate forced 29.970 fps at frame 42\n");
    EXPECT_EQ(english.format("media_video_timing_row", "0.03333", 2, 1, "12.500", "0.03333"),
              "Video timing fr=0.03333, tick=2, repeat=1, pts=12.500, step=0.03333\n");
    EXPECT_EQ(english.format("media_strange_video_pts_step", "0.06667", "0.03333", 42),
              "Strange video pts step of 0.06667 instead of 0.03333 at frame 42\n");
    EXPECT_STREQ(english.text("media_selftest_seek_failed"),
                 "\nSelftest 1 FAILED: Seektest\n:Starting test 3\n");
    EXPECT_EQ(spanish.format("media_framerate_forced", "25.000", 42),
              "Frecuencia de fotogramas forzada a 25.000 fps en el fotograma 42\n");
    EXPECT_STREQ(spanish.text("media_selftest_reopen_ok"),
                 "\nAutoprueba 3 CORRECTA: reapertura\n");
}
TEST(Translator, FormatsFrameTimingAndRecordingInputDiagnostics) {
    const Translator english;
    const Translator spanish("es");
    EXPECT_EQ(english.format("media_frame_rate_set", "29.970"), "Frame Rate set to 29.970 f/s\n");
    EXPECT_STREQ(spanish.text("media_no_stream_frame_rate"),
                 "Advertencia: no hay frecuencia de fotogramas en el flujo; se deriva del códec\n");
}
TEST(Translator, FormatsAudioAnalysisDiagnosticsWithStableEnglishLayout) {
    const Translator english;
    const Translator spanish("es");
    EXPECT_EQ(english.format("media_audio_channels_switched", "1.25000", 2, 6),
              "Audio channels switched at pts=1.25000 from 2 to 6\n");
    EXPECT_EQ(english.format("media_audio_base_pts_jump", "1.00000", "1.03200", "0.03200"),
              "Jump in base apts from 1.00000 to 1.03200, delta=0.03200\n");
    EXPECT_EQ(english.format("media_audio_strange_pts_step", "0.06667", "0.00000", 42),
              "Strange audio pts step of 0.06667 instead of 0.00000 at frame 42\n");
    EXPECT_EQ(english.format("media_initial_audio_pts", "     1.250"),
              "\nInitial audio pts =      1.250\n");
    EXPECT_EQ(spanish.format("media_ac3_skipped_bytes", 3, 100, 42),
              "Se omitieron 3 de 100 bytes añadidos en el flujo de audio cerca del fotograma 42\n");
}
TEST(Translator, FormatsSceneAnalysisDiagnosticsWithStableEnglishLayout) {
    const Translator english;
    const Translator spanish("es");
    EXPECT_EQ(english.format("scene_aspect_bounds", 42, "1.78", 1, 1079, 2, 1918),
              "Frame: 42\tRatio: 1.78\tMinY: 1 MaxY: 1079 MinX: 2 MaxX: 1918\n");
    EXPECT_EQ(english.format("scene_cutfile_saved", "    42", "sample.dmp"),
              "Saved frame     42 into cutfile \"sample.dmp\"\n");
    EXPECT_EQ(english.format("scene_black_frame", "    42", "1.250", 10, 20, 30),
              "Frame     42 (1.250s) - Black frame with brightness of 10,uniform of 20 and volume of 30\n");
    EXPECT_EQ(english.format("scene_resolution_change", "    42", "1.250", 720, 480, 1920, 1080),
              "Frame     42 (1.250s) - Resolution change from 720 x 480 to 1920 x 1080 \n");
    EXPECT_EQ(spanish.format("scene_audio_channels", 42, " 2"),
              "Fotograma: 42 Canales:  2\n");
    EXPECT_EQ(spanish.format("diag_invalid_scene_brightness", 256, 256),
              "Brillo de escena no válido 256 para 256 intervalos de histograma");
    EXPECT_EQ(spanish.format("scene_large_scene_change", "    42", "1.250", 12, 34),
              "Fotograma     42 (1.250s) - Fotograma negro por cambio grande de escena de 12, uniformidad 34\n");
}

TEST(Translator, FormatsLegacySettingsDiagnosticsWithStableEnglishLayout) {
    const Translator english;
    const Translator spanish("es");
    EXPECT_EQ(english.format("settings_input_files", "sample.mpg", "comskip.exe", "sample.logo", "sample.ini"),
              "Mpeg:\tsample.mpg\nExe\tcomskip.exe\nLogo:\tsample.logo\nIni:\tsample.ini\n");
    EXPECT_STREQ(english.text("settings_detection_methods"), "\nDetection Methods to be used:\n");
    EXPECT_EQ(english.format("settings_method_logo", 2, 60), "\t2) Logo - Give up after 60 seconds\n");
    EXPECT_EQ(english.format("settings_method_closed_captions", 5), "\t5) Closed Captions\n");
    EXPECT_STREQ(english.text("settings_heading"), "\nSettings\n--------\n");
    EXPECT_EQ(spanish.format("settings_input_files", "sample.mpg", "comskip.exe", "sample.logo", "sample.ini"),
              "Mpeg:\tsample.mpg\nExe\tcomskip.exe\nLogotipo:\tsample.logo\nINI:\tsample.ini\n");
    EXPECT_EQ(spanish.format("settings_method_logo", 2, 60), "\t2) Logotipo - abandonar después de 60 segundos\n");
    EXPECT_STREQ(spanish.text("settings_heading"), "\nConfiguración\n-------------\n");
}
