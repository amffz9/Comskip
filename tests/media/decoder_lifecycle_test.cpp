#include "recording_context.h"
#include "media/decoder.h"
#include "media/video_state.h"
#include "media/video_decode_status.h"
#include "media/video_packet_outcome.h"
#include "localization/diagnostic.h"
#include "platform/utf8_paths.h"
#include <gtest/gtest.h>
#include <chrono>
#include <fstream>
#include <memory>

namespace {
void expect_status_diagnostic(const std::runtime_error& error, comskip::diagnostics::Code code) {
    const auto* provider=dynamic_cast<const comskip::diagnostics::DiagnosticProvider*>(&error);
    ASSERT_NE(provider,nullptr); EXPECT_EQ(provider->diagnostic().code,code);
    ASSERT_EQ(provider->diagnostic().arguments.size(),1u);
    EXPECT_FALSE(provider->diagnostic().arguments.front().empty());
}
void closed(const VideoState& video) {
    EXPECT_EQ(video.video_st,nullptr); EXPECT_EQ(video.audio_st,nullptr); EXPECT_EQ(video.subtitle_st,nullptr);
    EXPECT_FALSE(video.videoStream); EXPECT_FALSE(video.audioStream); EXPECT_FALSE(video.subtitleStream);
    EXPECT_FALSE(video.pFormatCtx); EXPECT_FALSE(video.dec_ctx); EXPECT_FALSE(video.audio_ctx);
    EXPECT_FALSE(video.subtitle_ctx); EXPECT_FALSE(video.frame); EXPECT_FALSE(video.pFrame);
    EXPECT_FALSE(video.img_convert_ctx);
}
}
TEST(DecoderLifecycle, CloseWithoutVideoOwnerIsIdempotent) {
    auto context=std::make_unique<RecordingContext>();
    ASSERT_FALSE(context->state.video_owner);
    EXPECT_NO_THROW(file_close(*context));
    EXPECT_NO_THROW(file_close(*context));
    EXPECT_FALSE(context->state.video_owner);
}
TEST(VideoDecodeStatus, SendAcceptsSuccessAndOwnsEveryFfmpegFailureDetail) {
    EXPECT_NO_THROW(comskip::media::require_video_packet_sent(0));
    for (const int status : {AVERROR(EINVAL),AVERROR(EAGAIN),AVERROR_EOF}) {
        try { comskip::media::require_video_packet_sent(status); FAIL() << "expected failure"; }
        catch (const std::runtime_error& error) {
            expect_status_diagnostic(error,comskip::diagnostics::Code::send_video_packet_detail);
        }
    }
}
TEST(VideoDecodeStatus, ReceiveDistinguishesFrameRetryEofAndRealFailure) {
    EXPECT_EQ(comskip::media::classify_video_receive_status(0),comskip::media::VideoReceiveStatus::frame);
    EXPECT_EQ(comskip::media::classify_video_receive_status(42),comskip::media::VideoReceiveStatus::frame);
    EXPECT_EQ(comskip::media::classify_video_receive_status(AVERROR(EAGAIN)),comskip::media::VideoReceiveStatus::try_again);
    EXPECT_EQ(comskip::media::classify_video_receive_status(AVERROR_EOF),comskip::media::VideoReceiveStatus::end_of_stream);
    try { (void)comskip::media::classify_video_receive_status(AVERROR(EINVAL)); FAIL() << "expected failure"; }
    catch (const std::runtime_error& error) {
        expect_status_diagnostic(error,comskip::diagnostics::Code::receive_video_frame_detail);
    }
}
TEST(VideoPacketOutcome, TerminalStatesAreExplicitAndHaveStablePriority) {
    using enum comskip::media::VideoPacketOutcome;
    EXPECT_EQ(comskip::media::video_packet_outcome(false,false,false,false),no_frame);
    EXPECT_EQ(comskip::media::video_packet_outcome(true,false,false,false),frame_decoded);
    EXPECT_EQ(comskip::media::video_packet_outcome(true,true,false,false),analysis_complete);
    EXPECT_EQ(comskip::media::video_packet_outcome(true,true,true,false),selftest_complete);
    EXPECT_EQ(comskip::media::video_packet_outcome(true,true,true,true),positioning_failure);
}
TEST(DecoderLifecycle, MissingUnicodeInputOwnsCauseAndSameContextCanRetryValidMedia) {
    const auto directory=std::filesystem::temp_directory_path();
    const auto missing=directory/comskip::platform::path_from_utf8("missing-recording-café.y4m");
    const auto valid=directory/comskip::platform::path_from_utf8(
        "retry-recording-café-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".y4m");
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code error; std::filesystem::remove(path,error); } } cleanup{valid};
    auto context=std::make_unique<RecordingContext>();
    const auto missing_utf8=missing.u8string();
    context->state.mpegfilename={missing_utf8.begin(),missing_utf8.end()};
    context->settings.live_tv_retries=0; context->settings.verbose=0; context->settings.fps=25;
    try { file_open(*context); FAIL() << "expected missing recording failure"; }
    catch (const comskip::diagnostics::DiagnosticProvider& error) {
        EXPECT_EQ(error.diagnostic().code,comskip::diagnostics::Code::cannot_open_recording_detail);
        ASSERT_EQ(error.diagnostic().arguments.size(),2u);
        EXPECT_EQ(error.diagnostic().arguments[0],context->state.mpegfilename);
        EXPECT_FALSE(error.diagnostic().arguments[1].empty());
    }
    ASSERT_TRUE(context->state.video_owner); closed(*context->state.video_owner);
    { std::ofstream file(valid,std::ios::binary);
      file << "YUV4MPEG2 W160 H120 F25:1 Ip A1:1 C420jpeg\n";
      const std::string luma(160*120,80),chroma(160*120/2,static_cast<char>(128));
      for(int i=0;i<2;++i) file << "FRAME\n" << luma << chroma;
      ASSERT_TRUE(file.good()); }
    const auto valid_utf8=valid.u8string();
    context->state.mpegfilename={valid_utf8.begin(),valid_utf8.end()};
    ASSERT_NO_THROW(file_open(*context));
    ASSERT_TRUE(context->state.video_owner->pFormatCtx);
    ASSERT_TRUE(context->state.video_owner->videoStream);
    file_close(*context); closed(*context->state.video_owner);
}
TEST(DecoderLifecycle, ActualUnicodeMediaCloseClearsBorrowedReferencesAndReopens) {
    const auto path=std::filesystem::temp_directory_path()/comskip::platform::path_from_utf8(
        "decoder-café-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".y4m");
    struct Cleanup { std::filesystem::path path; ~Cleanup() {
        std::error_code error; std::filesystem::remove(path,error);
    }} cleanup{path};
    { std::ofstream file(path,std::ios::binary);
      file << "YUV4MPEG2 W160 H120 F25:1 Ip A1:1 C420jpeg\n";
      const std::string luma(160*120,80),chroma(160*120/2,static_cast<char>(128));
      for(int i=0;i<10;++i) file << "FRAME\n" << luma << chroma;
      ASSERT_TRUE(file.good()); }
    auto context=std::make_unique<RecordingContext>();
    const auto utf8=path.u8string();
    context->state.mpegfilename={utf8.begin(),utf8.end()};
    context->settings.fps=25; context->settings.verbose=0; context->settings.live_tv_retries=0;
    for(int pass=0;pass<2;++pass) {
        ASSERT_NO_THROW(file_open(*context));
        ASSERT_TRUE(context->state.video_owner);
        const auto& video=*context->state.video_owner;
    ASSERT_TRUE(video.pFormatCtx); ASSERT_TRUE(video.videoStream);
        ASSERT_NE(video.video_st,nullptr);
    EXPECT_EQ(video.video_st,video.pFormatCtx->streams[*video.videoStream]);
        EXPECT_TRUE(video.dec_ctx); EXPECT_TRUE(video.frame); EXPECT_TRUE(video.pFrame);
        EXPECT_EQ(video.video_st->codecpar->width,160);
        EXPECT_EQ(video.video_st->codecpar->height,120);
        EXPECT_NO_THROW(file_close(*context));
        ASSERT_TRUE(context->state.video_owner); closed(*context->state.video_owner);
        EXPECT_NO_THROW(file_close(*context)); closed(*context->state.video_owner);
    }
    EXPECT_TRUE(std::filesystem::remove(path));
}
TEST(DecoderLifecycle, OwnedSyntheticStreamsClearAllBorrowedPointersBeforeRepeatedClose) {
    auto context=std::make_unique<RecordingContext>();
    context->state.video_owner=std::make_unique<VideoState>();
    auto& video=*context->state.video_owner;
    video.pFormatCtx.reset(avformat_alloc_context()); ASSERT_TRUE(video.pFormatCtx);
    video.video_st=avformat_new_stream(video.pFormatCtx.get(),nullptr);
    video.audio_st=avformat_new_stream(video.pFormatCtx.get(),nullptr);
    video.subtitle_st=avformat_new_stream(video.pFormatCtx.get(),nullptr);
    ASSERT_NE(video.video_st,nullptr); ASSERT_NE(video.audio_st,nullptr); ASSERT_NE(video.subtitle_st,nullptr);
    video.videoStream=0; video.audioStream=1; video.subtitleStream=2;
    video.dec_ctx.reset(avcodec_alloc_context3(nullptr));
    video.audio_ctx.reset(avcodec_alloc_context3(nullptr));
    video.subtitle_ctx.reset(avcodec_alloc_context3(nullptr));
    video.frame=comskip::media::make_frame(); video.pFrame=comskip::media::make_frame();
    EXPECT_NO_THROW(file_close(*context)); closed(video);
    EXPECT_NO_THROW(file_close(*context)); closed(video);
}
TEST(DecoderLifecycle, ByteSeekWithoutIoContextReportsRangeDiagnostic) {
    auto context=std::make_unique<RecordingContext>();
    context->state.video_owner=std::make_unique<VideoState>();
    auto& video=*context->state.video_owner;
    video.pFormatCtx.reset(avformat_alloc_context());
    ASSERT_TRUE(video.pFormatCtx);
    ASSERT_EQ(video.pFormatCtx->pb,nullptr);
    video.seek_by_bytes=1;
    video.duration=100;
    try { Set_seek(*context,video,20); FAIL() << "expected unavailable byte seek failure"; }
    catch (const comskip::diagnostics::DiagnosticProvider& error) {
        EXPECT_EQ(error.diagnostic().code,comskip::diagnostics::Code::integer_range);
    }
    file_close(*context);
}
