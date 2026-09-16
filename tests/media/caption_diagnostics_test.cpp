#include "media/video_timestamp.h"
#include <limits>
#include "media/caption_session.h"
#include "media/subtitle_stream_decoder.h"
#include "review_window.h"
#include "diagnostic_render.h"
#include "media/a53_caption_bridge.h"
#include "media/audio_samples.h"
#include "profile.h"
#include "ini.h"
#include "media/ffmpeg_resources.h"
extern "C" {
#include <libavutil/frame.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
}
#include <gtest/gtest.h>
#include <chrono>
#include <array>

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
TEST(SupportDiagnostics, MalformedBridgePacketRetainsCategoryAndLocalizedReason) {
    const std::array<std::uint8_t, 1> payload{1};
    try { bridge_a53_captions(payload); FAIL() << "Accepted incomplete A53 triplet"; }
    catch (const std::invalid_argument& error) {
        translated(error, Code::malformed_a53_caption_triplets,
            "Malformed A53 caption triplets", "Tripletes de subtítulos A53 mal formados");
    }
}
TEST(SupportDiagnostics, DecoderEofRetainsLogicCategoryAndResetContract) {
    CaptionDecoder decoder; decoder.drain(1s);
    try { decoder.decode({}, 2s); FAIL() << "Accepted caption after drain"; }
    catch (const std::logic_error& error) {
        translated(error, Code::caption_decoder_must_be_reset_after_eof,
            "Caption decoder must be reset", "decodificador de subtítulos debe restablecerse");
    }
    decoder.reset(); EXPECT_NO_THROW(decoder.decode({}, 1s));
}
TEST(SupportDiagnostics, ProfileInvalidDurationsRetainOwnedSettingKey) {
    try { comskip::config::read_profile(comskip::config::Ini("commercial_lengths=0"),{}); FAIL() << "Accepted zero duration"; }
    catch (const std::invalid_argument& error) {
        translated(error, Code::profile_lengths_must_be_positive,
            "commercial_lengths must contain positive durations", "commercial_lengths debe contener duraciones positivas",
            {"commercial_lengths"});
    }
}
TEST(SupportDiagnostics, InvalidAudioFrameRetainsMalformedCategory) {
    AVFrame frame{};
    try { normalize_audio(frame); FAIL() << "Accepted empty audio frame"; }
    catch (const std::invalid_argument& error) {
        translated(error, Code::invalid_decoded_audio_frame,
            "Invalid decoded audio frame", "Trama de audio decodificada no válida");
    }
}
TEST(SupportDiagnostics, UnsupportedAudioChannelLayoutOwnsFfmpegFailureDetail) {
    auto frame=make_frame();
    frame->format=AV_SAMPLE_FMT_FLT; frame->sample_rate=48000; frame->nb_samples=1;
    // A valid decoded high-channel-count frame exceeds libswresample's
    // 64-channel conversion limit. Unknown mono is accepted when no remixing
    // is needed, so it does not exercise a library failure.
    frame->ch_layout.order=AV_CHANNEL_ORDER_UNSPEC;
    frame->ch_layout.nb_channels=65;
    ASSERT_EQ(av_channel_layout_check(&frame->ch_layout),1);
    std::array<float,65> samples{};
    std::uint8_t* plane=reinterpret_cast<std::uint8_t*>(samples.data());
    frame->extended_data=&plane;
    struct RestorePlanes { AVFrame* frame; ~RestorePlanes() { frame->extended_data=frame->data; } } restore{frame.get()};
    try { normalize_audio(*frame); FAIL() << "Accepted unsupported 65-channel conversion"; }
    catch (const std::runtime_error& error) {
        const auto* provider=dynamic_cast<const DiagnosticProvider*>(&error);
        ASSERT_NE(provider,nullptr);
        const auto code=provider->diagnostic().code;
        ASSERT_TRUE(code==Code::configure_audio_conversion_detail || code==Code::initialize_audio_conversion_detail);
        ASSERT_EQ(provider->diagnostic().arguments.size(),1u);
        const auto detail=provider->diagnostic().arguments[0];
        EXPECT_FALSE(detail.empty());
        frame->format=-1;
        const bool configuration=code==Code::configure_audio_conversion_detail;
        translated(error,code,
            configuration ? "Configure audio conversion" : "Initialize audio conversion",
            configuration ? "configurar la conversión de audio" : "inicializar la conversión de audio",{detail});
    }
}

TEST(CaptionDiagnostics, VideoTimeConversionRejectsUnrepresentableValuesAndClampsPreroll) {
    EXPECT_EQ(video_caption_timestamp(-1), CaptionTimestamp::zero());
    EXPECT_EQ(video_caption_timestamp(1.25), CaptionTimestamp(1250000));
    for (const double invalid : {std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::quiet_NaN(),
            static_cast<double>(std::numeric_limits<std::int64_t>::max()) / 1000000}) {
        try { video_caption_timestamp(invalid); FAIL() << "Accepted invalid timestamp"; }
        catch (const std::invalid_argument& error) {
            translated(error, Code::invalid_video_caption_timestamp,
                "Invalid video caption timestamp", "Marca de tiempo");
        }
    }
}
