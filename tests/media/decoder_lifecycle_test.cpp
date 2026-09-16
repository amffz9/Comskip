#include "recording_context.h"
#include "media/decoder.h"
#include "media/video_state.h"
#include <gtest/gtest.h>
#include <chrono>
#include <fstream>
#include <memory>

namespace {
void closed(const VideoState& video) {
    EXPECT_EQ(video.video_st,nullptr); EXPECT_EQ(video.audio_st,nullptr); EXPECT_EQ(video.subtitle_st,nullptr);
    EXPECT_EQ(video.videoStream,-1); EXPECT_EQ(video.audioStream,-1); EXPECT_EQ(video.subtitleStream,-1);
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
TEST(DecoderLifecycle, ActualUnicodeMediaCloseClearsBorrowedReferencesAndReopens) {
    const auto path=std::filesystem::temp_directory_path()/std::filesystem::u8path(
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
        ASSERT_TRUE(video.pFormatCtx); ASSERT_GE(video.videoStream,0);
        ASSERT_NE(video.video_st,nullptr);
        EXPECT_EQ(video.video_st,video.pFormatCtx->streams[video.videoStream]);
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
