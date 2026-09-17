#include "recording_context.h"
#include "media/decoder.h"
#include "media/audio_analysis.h"
#include "media/ffmpeg_resources.h"
#include "media/video_state.h"
#include "localization/diagnostic.h"
#include <gtest/gtest.h>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <random>


namespace {
void write_audio_fixture(const std::filesystem::path& path) {
    struct OutputCloser {
        void operator()(AVFormatContext* context) const {
            if (context->pb) avio_closep(&context->pb);
            avformat_free_context(context);
        }
    };
    AVFormatContext* raw = nullptr;
    ASSERT_GE(avformat_alloc_output_context2(&raw, nullptr, "wav", nullptr), 0);
    std::unique_ptr<AVFormatContext, OutputCloser> output(raw);
    ASSERT_TRUE(output);
    auto* stream = avformat_new_stream(output.get(), nullptr);
    ASSERT_TRUE(stream);
    stream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
    stream->codecpar->codec_id = AV_CODEC_ID_PCM_S16LE;
    stream->codecpar->format = AV_SAMPLE_FMT_S16;
    stream->codecpar->sample_rate = 48000;
    stream->codecpar->bits_per_coded_sample = 16;
    stream->codecpar->block_align = 2;
    av_channel_layout_default(&stream->codecpar->ch_layout, 1);
    stream->time_base = {1, 48000};
    const auto bytes = path.u8string();
    const std::string filename(bytes.begin(), bytes.end());
    ASSERT_GE(avio_open(&output->pb, filename.c_str(), AVIO_FLAG_WRITE), 0);
    ASSERT_GE(avformat_write_header(output.get(), nullptr), 0);
    auto packet = comskip::media::make_packet();
    ASSERT_TRUE(packet);
    ASSERT_GE(av_new_packet(packet.get(), 160), 0);
    std::memset(packet->data, 0, 160);
    packet->stream_index = stream->index;
    packet->pts = packet->dts = 0;
    packet->duration = 80;
    ASSERT_GE(av_interleaved_write_frame(output.get(), packet.get()), 0);
    ASSERT_GE(av_write_trailer(output.get()), 0);
}
class PlaybackWarnings : public ::testing::Test {
protected:
    std::filesystem::path directory;
    std::unique_ptr<RecordingContext> context;
    void SetUp() override {
        directory = std::filesystem::temp_directory_path() /
            ("comskip-playback-warning-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
             "-" + std::to_string(std::random_device{}()));
        ASSERT_TRUE(std::filesystem::create_directory(directory));
        context = std::make_unique<RecordingContext>();
        context->settings.verbose = 1;
        context->settings.live_tv_retries = 0;
        context->state.output_console = false;
        context->state.logfilename = (directory / "warning.log").string();
    }
    void TearDown() override {
        context.reset();
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }
    std::string log() {
        std::ifstream input(directory / "warning.log");
        return {std::istreambuf_iterator<char>(input), {}};
    }
    comskip::media::FramePtr audio_frame(int samples) {
        auto frame = comskip::media::make_frame();
        frame->format = AV_SAMPLE_FMT_S16;
        frame->sample_rate = 48000;
        frame->nb_samples = samples;
        av_channel_layout_default(&frame->ch_layout, 1);
        if (av_frame_get_buffer(frame.get(), 0) < 0) throw std::bad_alloc{};
        std::memset(frame->data[0], 0, static_cast<std::size_t>(samples) * sizeof(short));
        return frame;
    }
    void audio_stream(VideoState& video) {
        video.pFormatCtx.reset(avformat_alloc_context());
        ASSERT_TRUE(video.pFormatCtx);
        video.audio_st = avformat_new_stream(video.pFormatCtx.get(), nullptr);
        ASSERT_TRUE(video.audio_st);
        video.audio_st->codecpar->sample_rate = 48000;
        video.audio_st->codecpar->codec_id = AV_CODEC_ID_PCM_S16LE;
        video.audio_st->time_base = {1, 48000};
    }
};
TEST_F(PlaybackWarnings, InvalidDecodedFrameLogsSpanishAndDropsBorrowedPixels) {
    context->translator = comskip::localization::Translator("es");
    auto frame = comskip::media::make_frame();
    ASSERT_TRUE(frame);
    frame->width = 160;
    frame->height = 99;
    frame->linesize[0] = 160;
    unsigned char borrowed = 42;
    context->state.frame_ptr = &borrowed;
    EXPECT_EQ(SubmitFrame(*context, nullptr, frame.get(), 0), 0);
    EXPECT_EQ(context->state.frame_ptr, nullptr);
    EXPECT_EQ(context->state.frame_count, 0);
    EXPECT_EQ(log(), "Error: altura (99), anchura (160) o paso de fila (160) no válidos\n");
}
TEST_F(PlaybackWarnings, InvalidStrideUsesEnglishFallbackBeforeReadingPixels) {
    using comskip::config::Ini;
    context->translator = comskip::localization::Translator("es",
        Ini("media_invalid_frame=\"Panic: illegal height ({}), width ({}) or frame period ({})\\n\""), Ini{});
    auto frame = comskip::media::make_frame();
    ASSERT_TRUE(frame);
    frame->width = frame->height = 160;
    frame->linesize[0] = 99;
    EXPECT_EQ(SubmitFrame(*context, nullptr, frame.get(), 0), 0);
    EXPECT_EQ(context->state.frame_ptr, nullptr);
    EXPECT_EQ(log(), "Panic: illegal height (160), width (160) or frame period (99)\n");
}
TEST_F(PlaybackWarnings, ActualAudioOnlyFileReportsOwnedVideoFailureAndUnwinds) {
    context->translator = comskip::localization::Translator("es");
    const auto fixture = directory / "audio-only.wav";
    write_audio_fixture(fixture);
    context->state.mpegfilename = fixture.string();
    try { file_open(*context); FAIL() << "Expected rejection of audio-only media"; }
    catch (const comskip::diagnostics::DiagnosticProvider& error) {
        EXPECT_EQ(error.diagnostic().code,
                  comskip::diagnostics::Code::recording_has_no_decodable_video_stream);
        ASSERT_EQ(error.diagnostic().arguments.size(),1u);
        EXPECT_EQ(error.diagnostic().arguments.front(),context->state.mpegfilename);
    }
    EXPECT_TRUE(log().empty());
    ASSERT_TRUE(context->state.video_owner);
    EXPECT_FALSE(context->state.video_owner->pFormatCtx);
    context.reset();
    EXPECT_TRUE(std::filesystem::remove(fixture));
}
TEST_F(PlaybackWarnings, InconsistentAudioTimesReportSpanishAndResetBeforeWritingSamples) {
    context->translator = comskip::localization::Translator("es");
    VideoState video{};
    audio_stream(video);
    auto frame = audio_frame(2);
    context->state.sound_to_frames_old_sample_rate = 48000;
    context->state.base_apts = 1;
    context->state.top_apts = 0;
    context->state.audio_buffer[0] = 123;
    sound_to_frames(*context, video, *frame);
    EXPECT_EQ(log(), "Error: almacenamiento de audio incoherente\n");
    EXPECT_EQ(context->state.audio_buffer_ptr, std::data(context->state.audio_buffer));
    EXPECT_EQ(context->state.audio_samples, 0);
    EXPECT_EQ(context->state.base_apts, 0);
    EXPECT_EQ(context->state.top_apts, 0);
    EXPECT_EQ(context->state.audio_buffer[0], 123);
}
TEST_F(PlaybackWarnings, FullAudioBufferUsesEnglishFallbackWithoutOverwritingFinalSample) {
    using comskip::config::Ini;
    context->translator = comskip::localization::Translator("es",
        Ini("media_audio_buffer_overflow=\"Panic: Audio buffer overflow, resetting audio buffer\\n\""), Ini{});
    VideoState video{};
    audio_stream(video);
    auto frame = audio_frame(2);
    const auto last = std::size(context->state.audio_buffer) - 1;
    context->state.audio_buffer_ptr = std::data(context->state.audio_buffer) + last;
    context->state.audio_buffer[last] = 123;
    sound_to_frames(*context, video, *frame);
    EXPECT_EQ(log(), "Panic: Audio buffer overflow, resetting audio buffer\n");
    EXPECT_EQ(context->state.audio_buffer_ptr, std::data(context->state.audio_buffer));
    EXPECT_EQ(context->state.audio_samples, 0);
    EXPECT_EQ(context->state.base_apts, 0);
    EXPECT_EQ(context->state.top_apts, 0);
    EXPECT_EQ(context->state.audio_buffer[last], 123);
}
TEST_F(PlaybackWarnings, InvalidAc3StagingIndexLogsSpanishAndResetsBeforeArrayAccess) {
    context->translator = comskip::localization::Translator("es");
    context->settings.verbose = 8;
    context->state.reviewing = true;
    VideoState video{};
    audio_stream(video);
    video.audio_st->codecpar->codec_id = AV_CODEC_ID_AC3;
    context->settings.ALIGN_AC3_PACKETS = 1;
    context->state.ac3_packet_index = -1;
    std::uint8_t payload[] = {0x0b, 0x77};
    AVPacket packet{};
    packet.data = payload;
    packet.size = static_cast<int>(std::size(payload));
    audio_packet_process(*context, video, packet);
    EXPECT_EQ(log(), "Error de sincronización AC3\n");
    EXPECT_EQ(context->state.ac3_packet_index, 0);
}
}
