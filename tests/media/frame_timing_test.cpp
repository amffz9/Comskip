#include "recording_context.h"
#include <gtest/gtest.h>
#include <limits>
#include <memory>

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
