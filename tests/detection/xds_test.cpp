#include "recording_context.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <memory>
#include <string_view>

TEST(XdsBlocks, GrowsBeyondLegacyLimitAndPreservesIndependentMetadata) {
    auto first_owner = std::make_unique<RecordingContext>();
    auto second_owner = std::make_unique<RecordingContext>();
    auto& first = *first_owner;
    auto& second = *second_owner;
    first.state.frame.resize(2);
    first.state.framenum = 1;
    Init_XDS_block(first);
    first.state.XDS_block[0].duration = 42;
    const std::string_view title = "Owned program metadata";
    std::ranges::copy(title, first.state.XDS_block[0].name);
    for (int i = 0; i < 2200; ++i) Add_XDS_block(first);
    EXPECT_EQ(first.state.XDS_block_count, 2200);
    EXPECT_EQ(first.state.frame[1].xds, 2200);
    EXPECT_EQ(first.state.XDS_block[2200].duration, 42);
    EXPECT_STREQ(first.state.XDS_block[2200].name, "Owned program metadata");
    EXPECT_EQ(first.state.XDS_block[1999].frame, 1);
    Init_XDS_block(second);
    EXPECT_EQ(second.state.XDS_block_count, 0);
    EXPECT_EQ(second.state.XDS_block[0].duration, 0);
    EXPECT_STREQ(second.state.XDS_block[0].name, "");
}
TEST(XdsBlocks, RejectsFrameOutsideRecordingWithoutAdvancingMetadata) {
    auto owner = std::make_unique<RecordingContext>();
    auto& context = *owner;
    Init_XDS_block(context);
    context.state.framenum = -1;
    EXPECT_THROW(Add_XDS_block(context), std::out_of_range);
    EXPECT_EQ(context.state.XDS_block_count, 0);
}
TEST(CaptionGrids, OwnZeroInitializedScreenAndMemoryForEachRecording) {
    auto first_owner = std::make_unique<RecordingContext>();
    auto second_owner = std::make_unique<RecordingContext>();
    auto& first = *first_owner;
    auto& second = *second_owner;
    first.state.cc_screen[14][31] = 'X';
    first.state.cc_memory[0][0] = 'M';
    EXPECT_EQ(first.state.cc_screen[14][31], 'X');
    EXPECT_EQ(first.state.cc_screen[14][30], 0);
    EXPECT_EQ(first.state.cc_memory[14][31], 0);
    EXPECT_EQ(second.state.cc_screen[14][31], 0);
    EXPECT_EQ(second.state.cc_memory[0][0], 0);
}
