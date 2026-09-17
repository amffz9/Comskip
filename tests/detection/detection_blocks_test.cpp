#include "recording_context.h"
#include "black_frame_run.h"
#include <gtest/gtest.h>
#include <array>
#include <cmath>
#include <memory>

namespace {
std::unique_ptr<RecordingContext> observations(int separators) {
    auto context = std::make_unique<RecordingContext>();
    context->settings.verbose = 0;
    context->settings.commDetectMethod = BLACK_FRAME;
    context->settings.fps = 25;
    context->settings.intelligent_brightness = false;
    context->settings.min_black_frames_for_break = 1;
    context->state.frame_count = context->state.framesprocessed = (separators + 1) * 50;
    context->state.framenum_real = context->state.frame_count + 1;
    context->state.frame.resize(context->state.frame_count + 1);
    for (long frame = 1; frame <= context->state.frame_count; ++frame)
        context->state.frame[frame].pts = (frame - 1) / context->settings.fps;
    for (int separator = 1; separator <= separators; ++separator)
        InsertBlackFrame(*context, separator * 50, 0, 0, 0, C_b);
    return context;
}

TEST(BlackFrameRun, StopsAtLastActiveObservationWithoutInspectingStorageAfterSpan) {
    const std::array storage{
        black_frame_info{41, 0, 0, 0, C_b},
        black_frame_info{42, 0, 0, 0, C_b}, // Contiguous poison outside active range.
    };

    EXPECT_EQ(comskip::detection::contiguous_black_frame_run_end(
                  std::span<const black_frame_info>{storage}.first(1), 0, C_b),
              0u);
}

TEST(BlackFrameRun, ExtendsOnlyAcrossMatchingContiguousActiveObservations) {
    const std::array frames{
        black_frame_info{41, 0, 0, 0, C_b},
        black_frame_info{42, 0, 0, 0, C_b | C_s},
        black_frame_info{44, 0, 0, 0, C_b},
    };

    EXPECT_EQ(comskip::detection::contiguous_black_frame_run_end(frames, 0, C_b), 1u);
    EXPECT_EQ(comskip::detection::contiguous_black_frame_run_end(frames, frames.size(), C_b),
              frames.size());
}
void terminal(const RecordingContext& context) {
    ASSERT_EQ(context.state.cblock.size(), context.state.block_count + 1);
    const auto& last = context.state.cblock.back();
    EXPECT_EQ(last.f_start, 0);
    EXPECT_EQ(last.f_end, 0);
    EXPECT_EQ(last.b_head, 0);
    EXPECT_EQ(last.b_tail, 0);
    EXPECT_EQ(last.strict, 0);
    EXPECT_EQ(last.iscommercial, 0);
    EXPECT_DOUBLE_EQ(last.score, 1.0);
}
}

TEST(DetectionBlocks, BuildsBeyondFormerLimitWithoutLosingBoundaries) {
    auto context = observations(1200);
    ASSERT_TRUE(BuildBlocks(*context, true));
    ASSERT_EQ(context->state.block_count, 1201);
    for (long block = 0; block < context->state.block_count; ++block) {
        const long expected_end = block == context->state.block_count - 1
            ? context->state.framesprocessed : (block + 1) * 50 - 1;
        EXPECT_EQ(context->state.cblock[block].f_end, expected_end);
        if (block > 0)
            EXPECT_EQ(context->state.cblock[block].f_start,
                      context->state.cblock[block - 1].f_end + 1);
    }
    terminal(*context);
}

TEST(DetectionBlocks, BlackBoundaryMergingPreservesInitializedTerminal) {
    auto context = observations(8);
    context->settings.min_black_frames_for_break = 2;
    ASSERT_TRUE(BuildBlocks(*context, true));
    ASSERT_EQ(context->state.block_count, 1);
    EXPECT_EQ(context->state.cblock[0].f_start, 1);
    EXPECT_EQ(context->state.cblock[0].f_end, context->state.framesprocessed);
    terminal(*context);
}

TEST(DetectionBlocks, ValidationDoesNotReadPastTheLastActiveBlackFrame) {
    auto context = observations(2);
    context->state.black.resize(context->state.black_count);
    context->state.black.shrink_to_fit();
    ASSERT_EQ(context->state.black.size(), 2u);
    ASSERT_EQ(context->state.black_count, 2);

    ASSERT_TRUE(BuildBlocks(*context, true));
    EXPECT_GT(context->state.block_count, 0);
    terminal(*context);
}

TEST(DetectionBlocks, InitializationClearsPreviousClassificationAndResetPreservesContract) {
    auto context = observations(2);
    ASSERT_TRUE(BuildBlocks(*context, true));
    ASSERT_EQ(context->state.block_count, 3);
    auto& block = context->state.cblock[0];
    block.strict = 2; block.iscommercial = 1; block.cc_type = 4;
    block.reffer = 'C'; block.logo = 0.9; block.correlation = 123;
    InitializeBlockArray(*context, 0);
    EXPECT_EQ(block.strict, 0); EXPECT_EQ(block.iscommercial, 0);
    EXPECT_EQ(block.cc_type, 0); EXPECT_EQ(block.reffer, 0);
    EXPECT_DOUBLE_EQ(block.logo, 0); EXPECT_DOUBLE_EQ(block.correlation, 0);
    comskip::detection::reset_blocks(context->state.cblock, context->state.block_count);
    EXPECT_EQ(context->state.block_count, 0);
    terminal(*context);
}

TEST(DetectionBlocks, LogoJoiningPreservesTerminalAndCompleteRecordingBoundary) {
    auto context = observations(3);
    ASSERT_TRUE(BuildBlocks(*context, true));
    ASSERT_EQ(context->state.block_count, 4);
    context->settings.commDetectMethod |= LOGO;
    context->settings.connect_blocks_with_logo = true;
    context->state.logo_block_count = 1;
    context->state.logo_block.resize(1);
    context->state.logo_block[0] = {1, static_cast<int>(context->state.framesprocessed)};
    CleanLogoBlocks(*context);
    ASSERT_EQ(context->state.block_count, 1);
    EXPECT_EQ(context->state.cblock[0].f_start, 1);
    EXPECT_EQ(context->state.cblock[0].f_end, context->state.framesprocessed);
    terminal(*context);
}

TEST(DetectionBlocks, AspectJoiningKeepsFinalStandardLengthScoreAndTerminal) {
    auto context = observations(0);
    context->state.frame_count = context->state.framesprocessed = 751;
    context->state.framenum_real = 752;
    context->state.frame.resize(752);
    for (int frame = 1; frame <= 751; ++frame)
        context->state.frame[frame].pts = (frame - 1) / context->settings.fps;
    for (int index = 0; index < 3; ++index) {
        auto& block = context->state.cblock[index];
        block.f_start = index * 250 + 1;
        block.f_end = index == 2 ? 751 : (index + 1) * 250;
        block.length = index == 1 ? 2 : 14;
        block.ar_ratio = 1.5;
        block.cause = C_a;
        comskip::detection::complete_block(context->state.cblock, context->state.block_count);
    }
    WeighBlocks(*context);
    ASSERT_EQ(context->state.block_count, 1);
    EXPECT_EQ(context->state.cblock[0].f_end, 751);
    EXPECT_DOUBLE_EQ(context->state.cblock[0].length, 30);
    EXPECT_EQ(context->state.cblock[0].strict, 2);
    terminal(*context);
}

TEST(DetectionBlocks, EmptyScoringAndFinalCommercialLengthAreSafe) {
    auto context = observations(0);
    EXPECT_NO_THROW(WeighBlocks(*context));
    terminal(*context);
    context->state.cblock[0].f_start = 1;
    context->state.cblock[0].f_end = 751;
    context->state.cblock[0].length = 30;
    context->state.cblock[0].cause = C_b;
    context->state.frame_count = context->state.framesprocessed = 751;
    context->state.framenum_real = 752;
    context->state.frame.resize(752);
    for (int frame = 1; frame <= 751; ++frame)
        context->state.frame[frame].pts = (frame - 1) / context->settings.fps;
    comskip::detection::complete_block(context->state.cblock, context->state.block_count);
    EXPECT_NO_THROW(WeighBlocks(*context));
    EXPECT_EQ(context->state.cblock[0].strict, 2);
    EXPECT_TRUE(std::isfinite(context->state.cblock[0].score));
    terminal(*context);
}

TEST(DetectionBlocks, PunishmentOrderingGrowsBeyondLegacyFixedCapacity) {
    auto context = observations(2100);
    ASSERT_TRUE(BuildBlocks(*context, true));
    ASSERT_GT(context->state.block_count, 2000);
    context->state.cblock[2000].length = 10'000;

    ASSERT_NO_THROW(BuildPunish(*context));
    ASSERT_EQ(context->state.length_order.size(),
              static_cast<std::size_t>(context->state.block_count));
    EXPECT_EQ(context->state.length_order.front(), 2000);
}
