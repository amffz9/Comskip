#include "recording_context.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <memory>
#include <stdexcept>
#include <vector>

bool CheckSceneHasChanged(RecordingContext& context);
namespace {
std::unique_ptr<RecordingContext> scene_context(int width, int height, int stride, int border) {
    auto context = std::make_unique<RecordingContext>();
    context->settings.commDetectMethod = 0;
    context->settings.thread_count = 1;
    context->settings.border = border;
    context->settings.max_brightness = 255;
    context->settings.test_brightness = 0;
    auto& state = context->state;
    state.videowidth = width;
    state.width = stride;
    state.height = height;
    state.frame_count = state.framenum_real = 1;
    state.frame.resize(3);
    state.ac_block.resize(1);
    state.ar_block.resize(1);
    state.ensure_pixel_buffers(false);
    return context;
}
TEST(SceneSamplingApplication, ZeroBorderUsesOnlyVisiblePixelsWithPadding) {
    auto context = scene_context(4,4,6,0);
    std::vector<unsigned char> image(24,255);
    for (int y=0;y<4;++y) std::fill_n(image.begin()+y*6,4,7);
    context->state.frame_ptr = image.data();
    EXPECT_NO_THROW(CheckSceneHasChanged(*context));
    EXPECT_GT(context->state.histogram[7],0);
    EXPECT_EQ(context->state.histogram[255],0);
    EXPECT_EQ(image[23],255);
}
TEST(SceneSamplingApplication, SmallValidImageAndLastUsefulBorderAreSupported) {
    for (auto dimensions : {std::array{2,2,0},std::array{4,4,1}}) {
        auto context = scene_context(dimensions[0],dimensions[1],dimensions[0],dimensions[2]);
        std::vector<unsigned char> image(dimensions[0]*dimensions[1],7);
        context->state.frame_ptr = image.data();
        EXPECT_NO_THROW(CheckSceneHasChanged(*context));
        EXPECT_GT(context->state.histogram[7],0);
    }
}
TEST(SceneSamplingApplication, InvalidBorderAndBrightnessFailBeforeHistogramChanges) {
    for (int kind=0;kind<3;++kind) {
        auto context = scene_context(4,4,4,0);
        std::vector<unsigned char> image(16,7);
        context->state.frame_ptr = image.data();
        context->state.histogram[7] = 91;
        context->state.brightness = 31;
        if (kind==0) context->settings.border=2;
        if (kind==1) context->settings.max_brightness=256;
        if (kind==2) context->settings.test_brightness=-1;
        EXPECT_THROW(CheckSceneHasChanged(*context),std::invalid_argument);
        EXPECT_EQ(context->state.histogram[7],91);
        EXPECT_EQ(context->state.brightness,31);
        EXPECT_EQ(image,std::vector<unsigned char>(16,7));
    }
}
TEST(SceneSamplingApplication, FullyMaskedHistogramRemainsZeroOnLaterFrame) {
    auto context = scene_context(4,4,4,0);
    std::vector<unsigned char> image(16,7);
    context->state.frame_ptr = image.data();
    std::ranges::fill(context->state.haslogo,1);
    EXPECT_NO_THROW(CheckSceneHasChanged(*context));
    context->state.framenum_real = context->state.frame_count = 2;
    EXPECT_NO_THROW(CheckSceneHasChanged(*context));
    EXPECT_EQ(context->state.brightness,0);
    EXPECT_EQ(context->state.sceneChangePercent,0);
    EXPECT_EQ(context->state.histogram[7],0);
}
TEST(SceneSamplingApplication, MissingBuffersFailBeforeSampling) {
    auto context = scene_context(4,4,4,0);
    context->state.histogram[7] = 91;
    EXPECT_THROW(CheckSceneHasChanged(*context),std::invalid_argument);
    std::vector<unsigned char> image(16,7);
    context->state.frame_ptr = image.data();
    context->state.haslogo.resize(15);
    EXPECT_THROW(CheckSceneHasChanged(*context),std::invalid_argument);
    EXPECT_EQ(context->state.histogram[7],91);
}
}
