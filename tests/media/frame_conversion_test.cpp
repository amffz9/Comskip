#include "frame_conversion.h"
#include <gtest/gtest.h>
#include <cstdint>
#include <memory>
extern "C" {
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
}

TEST(FrameConversion, ReusesFrameObjectAndPreservesPropertiesAcrossConversions)
{
    const auto release_frame = [](AVFrame* frame) { av_frame_free(&frame); };
    std::unique_ptr<AVFrame, decltype(release_frame)> frame(av_frame_alloc(), release_frame);
    comskip::media::ScalerPtr context;
    ASSERT_NE(frame, nullptr);
    AVFrame* const original = frame.get();
    for (int iteration = 0; iteration < 100; ++iteration) {
        av_frame_unref(frame.get());
        frame->format = AV_PIX_FMT_YUV420P10LE;
        frame->width = iteration % 2 ? 32 : 64;
        frame->height = 32;
        frame->pts = iteration;
        frame->best_effort_timestamp = iteration + 100;
        ASSERT_GE(av_frame_get_buffer(frame.get(), 0), 0);
        for (int plane = 0; plane < 3; ++plane) {
            const int width = plane == 0 ? frame->width : frame->width / 2;
            const int height = plane == 0 ? frame->height : frame->height / 2;
            for (int row = 0; row < height; ++row) {
                auto* samples = reinterpret_cast<std::uint16_t*>(frame->data[plane] + row * frame->linesize[plane]);
                for (int column = 0; column < width; ++column)
                    samples[column] = 512;
            }
        }
        const int result = comskip::media::convert_frame_to_8bit(*frame, context);
        ASSERT_EQ(result, 0);
        EXPECT_EQ(frame.get(), original);
        EXPECT_EQ(frame->format, AV_PIX_FMT_YUV420P);
        EXPECT_EQ(frame->pts, iteration);
        EXPECT_EQ(frame->best_effort_timestamp, iteration + 100);
        EXPECT_NEAR(frame->data[0][0], 128, 1);
    }
}

TEST(FrameConversion, RejectsUnsupportedInputWithoutChangingFrame)
{
    AVFrame* frame = av_frame_alloc();
    ASSERT_NE(frame, nullptr);
    frame->format = AV_PIX_FMT_YUV420P;
    frame->width = frame->height = 32;
    frame->pts = 7;
    comskip::media::ScalerPtr context;
    EXPECT_LT(comskip::media::convert_frame_to_8bit(*frame, context), 0);
    EXPECT_EQ(frame->format, AV_PIX_FMT_YUV420P);
    EXPECT_EQ(frame->pts, 7);
    EXPECT_EQ(context, nullptr);
    av_frame_free(&frame);
}
