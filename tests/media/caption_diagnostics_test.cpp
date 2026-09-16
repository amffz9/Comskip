#include "media/caption_session.h"
#include "media/subtitle_stream_decoder.h"
#include "review_window.h"
#include "diagnostic_render.h"
#include <gtest/gtest.h>
#include <chrono>

namespace {
using namespace comskip::media;
using namespace comskip::diagnostics;
using namespace std::chrono_literals;
void translated(const std::exception& error, Code code, std::string_view english,
                std::string_view spanish, const std::vector<std::string>& arguments = {}) {
    const auto* provider = dynamic_cast<const DiagnosticProvider*>(&error);
    ASSERT_NE(provider, nullptr);
    EXPECT_EQ(provider->diagnostic().code, code);
    EXPECT_EQ(provider->diagnostic().arguments, arguments);
    EXPECT_NE(comskip::localization::render_exception(error, comskip::localization::Translator("en")).find(english), std::string::npos);
    EXPECT_NE(comskip::localization::render_exception(error, comskip::localization::Translator("es")).find(spanish), std::string::npos);
}
}
TEST(CaptionDiagnostics, OrderingPreservesExceptionTypeAndOwnedTimestampArguments) {
    CaptionSession session({});
    session.consume({}, 2s);
    try { session.consume({}, 1s); FAIL() << "Accepted reversed caption time"; }
    catch (const std::invalid_argument& error) {
        session.reset();
        translated(error, Code::caption_consume_time_precedes_previous,
            "Caption consume time 1000000 precedes 2000000", "consumo de subtítulos 1000000 precede a 2000000",
            {"1000000", "2000000"});
    }
}
TEST(CaptionDiagnostics, EofLifecycleRemainsLogicErrorAndResetRestoresUse) {
    CaptionSession session({}); session.finish(2s);
    try { session.consume({}, 3s); FAIL() << "Accepted caption after EOF"; }
    catch (const std::logic_error& error) {
        translated(error, Code::caption_session_must_be_reset_after_eof,
            "Caption session must be reset", "sesión de subtítulos debe restablecerse");
    }
    session.reset(); EXPECT_NO_THROW(session.consume({}, 1s));
}
TEST(CaptionDiagnostics, BitmapRejectionRetainsActionableLocalizedPolicy) {
    AVCodecParameters source{}; source.codec_type = AVMEDIA_TYPE_SUBTITLE;
    source.codec_id = AV_CODEC_ID_DVD_SUBTITLE;
    try { SubtitleStreamDecoder decoder(source, {1, 1000}); FAIL() << "Accepted bitmap subtitle"; }
    catch (const std::invalid_argument& error) {
        translated(error, Code::bitmap_subtitle_streams_cannot_produce_srt_sami_text_without_ocr_select_a_text_subtitle_stream,
            "select a text subtitle stream", "seleccione un flujo de subtítulos de texto");
    }
}
TEST(CaptionDiagnostics, ReviewInvariantUsesLocalizedTypedErrorWithoutMutatingInput) {
    comskip::ui::ReviewWindow window;
    window.input().key = 'W';
    try { window.configure_font({}, 0); FAIL() << "Accepted zero font size"; }
    catch (const std::invalid_argument& error) {
        translated(error, Code::review_font_size_must_be_positive,
            "Review font size must be positive", "tamaño de la fuente de revisión debe ser positivo");
    }
    EXPECT_EQ(window.input().key, 'W'); EXPECT_EQ(window.options().font_size, 16);
}
