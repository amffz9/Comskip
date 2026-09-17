#include "recording_context.h"
#include "app/comskip.h"
#include "detection/logo_detection.h"
#include "image_geometry.h"

#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>

TEST(PixelBuffers, AllocatesOnlyCurrentGeometryAndOnlyRequestedFeatures)
{
    auto context = std::make_unique<RecordingContext>();
    auto& state = context->state;
    EXPECT_TRUE(state.haslogo.empty());
    EXPECT_TRUE(state.graph.empty());
    state.width = 160;
    state.height = 120;
    state.ensure_pixel_buffers(false);
    EXPECT_EQ(state.haslogo.size(), 160u * 120);
    EXPECT_TRUE(state.horiz_count.empty());
    EXPECT_TRUE(state.min_br.empty());
    state.ensure_pixel_buffers(true);
    EXPECT_EQ(state.horiz_count.size(), state.haslogo.size());
    EXPECT_EQ(state.cvert_edgemask.size(), state.haslogo.size());
    EXPECT_TRUE(std::ranges::all_of(state.min_br, [](auto value) { return value == 255; }));
    state.ensure_review_graph(320, 272);
    EXPECT_EQ(state.graph.size(), 320u * 272 * 3);
}

TEST(PixelBuffers, GeometryChangesResetMasksAndLogoHistoryEvenWithTheSamePixelCount)
{
    auto context = std::make_unique<RecordingContext>();
    auto& state = context->state;
    state.width = 160;
    state.height = 120;
    state.ensure_pixel_buffers(true);
    state.haslogo[0] = 1;
    state.choriz_edgemask[0] = 1;
    state.logoFrameBuffer.assign(2, std::vector<unsigned char>(160u * 120, 127));
    state.logoFrameNum = {25, 50};
    state.logoInfoAvailable = state.logoBuffersFull = true;
    state.newestLogoBuffer = 1;
    state.width = 120;
    state.height = 160;
    state.ensure_pixel_buffers(true);
    EXPECT_EQ(state.haslogo[0], 0);
    EXPECT_EQ(state.choriz_edgemask[0], 0);
    EXPECT_TRUE(state.logoFrameBuffer.empty());
    EXPECT_TRUE(state.logoFrameNum.empty());
    EXPECT_FALSE(state.logoInfoAvailable);
    EXPECT_FALSE(state.logoBuffersFull);
    EXPECT_FALSE(state.newestLogoBuffer);
    state.haslogo[0] = 1;
    state.ensure_pixel_buffers(true);
    EXPECT_EQ(state.haslogo[0], 1) << "An unchanged geometry must retain its learned mask";
    state.width = state.height = 100;
    state.ensure_pixel_buffers(false);
    EXPECT_EQ(state.haslogo.size(), 10000u);
    EXPECT_TRUE(state.horiz_count.empty());
}

TEST(PixelBuffers, RejectsInvalidAndOverflowingDimensions)
{
    using comskip::detection::checked_image_size;
    EXPECT_THROW(checked_image_size(0, 10), std::invalid_argument);
    EXPECT_THROW(checked_image_size(10, -1), std::invalid_argument);
    EXPECT_THROW(checked_image_size(10, 10, 0), std::invalid_argument);
    EXPECT_THROW(checked_image_size(2, 2, std::numeric_limits<std::size_t>::max()), std::length_error);
    auto context = std::make_unique<RecordingContext>();
    context->state.width = max_width + 1;
    context->state.height = 120;
    EXPECT_THROW(context->state.ensure_pixel_buffers(true), std::invalid_argument);
    EXPECT_TRUE(context->state.haslogo.empty());
}

TEST(PixelBuffers, LogoRingResizesForGeometryAndConfiguredBufferCount)
{
    auto context = std::make_unique<RecordingContext>();
    auto& state = context->state;
    state.width = state.videowidth = 160;
    state.height = 120;
    context->settings.num_logo_buffers = 2;
    InitLogoBuffers(*context);
    ASSERT_EQ(state.logoFrameBuffer.size(), 2u);
    ASSERT_EQ(state.logoFrameNum.size(), 2u);
    EXPECT_EQ(state.logoFrameBuffer[0].size(), 160u * 120);
    state.logoFrameNum[0] = 25;
    state.logoBuffersFull = true;
    context->settings.num_logo_buffers = 3;
    InitLogoBuffers(*context);
    EXPECT_EQ(state.logoFrameBuffer.size(), 3u);
    EXPECT_EQ(state.logoFrameNum.size(), 3u);
    EXPECT_TRUE(std::ranges::all_of(state.logoFrameNum, [](auto value) { return value == 0; }));
    EXPECT_FALSE(state.logoBuffersFull);
    state.width = state.videowidth = 192;
    state.height = 144;
    InitLogoBuffers(*context);
    ASSERT_EQ(state.logoFrameBuffer.size(), 3u);
    EXPECT_EQ(state.logoFrameBuffer[0].size(), 192u * 144);
    EXPECT_EQ(state.logoFrameBufferSize, 192 * 144);
    context->settings.num_logo_buffers = 0;
    EXPECT_THROW(InitLogoBuffers(*context), std::invalid_argument);
}

TEST(PixelBuffers, BorderlessRoundedReviewSamplingStaysInsideDecodedPixels)
{
    const std::vector<unsigned char> pixels(192u * 145, 127);
    const comskip::detection::LumaImageView image(pixels, 192, 160, 145);
    EXPECT_EQ(image.scaled_sample(639, 579, 0.25), 127);
    EXPECT_EQ(image.scaled_sample(640, 579, 0.25), 0) << "Do not render row padding as image pixels";
    EXPECT_EQ(image.scaled_sample(639, 580, 0.25), 0) << "A rounded canvas extends past decoded rows";
    EXPECT_EQ(image.scaled_sample(639, 607, 0.25), 0);
    EXPECT_EQ(image.scaled_sample(-1, 0, 0.25), 0);
    EXPECT_EQ(image.scaled_sample(0, 0, std::numeric_limits<double>::infinity()), 0);
    EXPECT_THROW((comskip::detection::LumaImageView(std::span{pixels}.first(10), 192, 160, 145)), std::invalid_argument);
}

TEST(PixelBuffers, RejectsPersistedLogoBoundsBeforeReadingMaskPixels)
{
    using comskip::detection::validate_logo_bounds;
    EXPECT_NO_THROW(validate_logo_bounds(160, 120, 0, 159, 0, 119));
    EXPECT_THROW(validate_logo_bounds(160, 120, -1, 10, 0, 10), std::invalid_argument);
    EXPECT_THROW(validate_logo_bounds(160, 120, 0, 160, 0, 10), std::invalid_argument);
    EXPECT_THROW(validate_logo_bounds(160, 120, 10, 5, 0, 10), std::invalid_argument);
    const auto path = std::filesystem::temp_directory_path() /
        ("comskip invalid bounds " + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".logo.txt");
    {
        std::ofstream file(path);
        ASSERT_TRUE(file);
        file << "picWidth=160\npicHeight=120\nlogoMinX=0\nlogoMaxX=160\nlogoMinY=0\nlogoMaxY=10\n";
    }
    auto context = std::make_unique<RecordingContext>();
    const auto bytes = path.u8string();
    context->state.logofilename.assign(bytes.begin(), bytes.end());
    EXPECT_THROW(LoadLogoMaskData(*context), std::invalid_argument);
    EXPECT_FALSE(context->state.logoInfoAvailable);
    EXPECT_NO_THROW(std::filesystem::remove(path));
}
