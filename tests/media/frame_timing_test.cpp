#include "recording_context.h"
#include "detection/frame_timestamps.h"
#include <gtest/gtest.h>
#include <limits>
#include <memory>

TEST(FrameTime, FallsBackWithoutObservationsAndBoundsInconsistentCounts) {
    auto context = std::make_unique<RecordingContext>();
    context->settings.fps = 25;
    context->state.frame.resize(3);
    context->state.frame_count = 0;
    EXPECT_DOUBLE_EQ(get_frame_pts(*context, 25), 1);
    context->state.frame_count = std::numeric_limits<int>::min();
    EXPECT_DOUBLE_EQ(get_frame_pts(*context, 25), 1);
    context->state.frame_count = std::numeric_limits<int>::max();
    context->state.frame[1].pts = 0.4;
    context->state.frame[2].pts = 0.8;
    EXPECT_DOUBLE_EQ(get_frame_pts(*context, -10), 0.4);
    EXPECT_DOUBLE_EQ(get_frame_pts(*context, std::numeric_limits<int>::max()), 0.8);
}

TEST(FrameVolume, IgnoresTerminalAndUnrepresentableIndicesWithoutTouchingStorage) {
    auto context = std::make_unique<RecordingContext>();
    auto& state = context->state;
    state.initialized = true;
    state.framearray = true;
    state.frame_count = 2;
    state.frame.resize(2);
    state.frame[1].brightness = 20;
    state.frame[1].volume = 17;
    set_frame_volume(*context, 2, 100);
    set_frame_volume(*context, std::numeric_limits<unsigned int>::max(), 100);
    EXPECT_EQ(state.frame[1].volume, 17);
    EXPECT_EQ(state.volumeHistogram[10], 0);
    EXPECT_EQ(state.silenceHistogram[100], 0);
}

TEST(FrameVolume, UpdatesValidFrameAndMatchingBlackObservation) {
    auto context = std::make_unique<RecordingContext>();
    auto& state = context->state;
    state.initialized = true;
    state.framearray = true;
    state.frame_count = 2;
    state.frame.resize(2);
    state.frame[1].brightness = 20;
    state.black_count = 1;
    state.black.resize(1);
    state.black[0].frame = 1;
    state.black[0].brightness = 20;
    set_frame_volume(*context, 1, 100);
    EXPECT_EQ(state.frame[1].volume, 100);
    EXPECT_EQ(state.black[0].volume, 100);
    EXPECT_EQ(state.volumeHistogram[10], 1);
    EXPECT_EQ(state.silenceHistogram[100], 1);
}
