#include "recording_context.h"
#include "legacy_detection.h" // Legacy detector fixture entry points and bit patterns.
#include "input/reference_file.h"
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>

namespace {
TEST(CommercialStorage, ActualBlockProducerPreservesMoreThanOneHundredThousandRuns) {
    auto context = std::make_unique<RecordingContext>();
    context->settings.verbose = 0;
    context->settings.fps = 25;
    context->settings.global_threshold = 1;
    constexpr int runs = 100001;
    context->state.block_count = runs * 2 - 1;
    context->state.cblock.resize(context->state.block_count + 1, comskip::detection::empty_block());
    for (int index = 0; index < context->state.block_count; ++index) {
        auto& block = context->state.cblock[index];
        block.f_start = index * 2 + 1; block.f_end = index * 2 + 2;
        block.score = index % 2 ? 0 : 2;
        block.iscommercial = true; // A rebuild must clear previously classified show blocks.
    }
    BuildCommercial(*context);
    ASSERT_EQ(context->state.commercial.size(), runs);
    ASSERT_EQ(context->state.commercial_count, runs - 1);
    for (int index = 0; index < runs; ++index) {
        const auto& interval = context->state.commercial[index];
        ASSERT_EQ(interval.start_frame, index * 4 + 1);
        ASSERT_EQ(interval.end_frame, index * 4 + 2);
        ASSERT_EQ(interval.start_block, index * 2);
        ASSERT_EQ(interval.end_block, index * 2);
        if (index + 1 < runs) EXPECT_FALSE(context->state.cblock[index * 2 + 1].iscommercial);
    }
    context->state.block_count = 0;
    context->state.cblock.resize(1);
    BuildCommercial(*context);
    EXPECT_TRUE(context->state.commercial.empty());
    EXPECT_EQ(context->state.commercial_count, -1);
}

TEST(CommercialStorage, ReferenceParserDefaultAcceptsMoreThanFormerCapacity) {
    std::stringstream source;
    source << "FILE PROCESSING COMPLETE 400004 FRAMES AT 2500\n-------------------\n";
    for (int index = 0; index < 100001; ++index)
        source << index * 4 << ' ' << index * 4 + 3 << '\n';
    const auto parsed = comskip::input::read_reference_file(source);
    ASSERT_EQ(parsed.intervals.size(), 100001u);
    EXPECT_EQ(parsed.intervals.front().start_frame, 0);
    EXPECT_EQ(parsed.intervals.back().end_frame, 400003);
}

TEST(CommercialStorage, ReviewInsertionUsesUncoveredGapAndRecordingBounds) {
    std::vector<Legacy_reffer_entry> intervals;
    int last = -1;
    ASSERT_TRUE(comskip::detection::insert_reference(intervals, last, 2500, 5000));
    EXPECT_EQ(intervals[0].start_frame, 1500); EXPECT_EQ(intervals[0].end_frame, 3500);
    ASSERT_TRUE(comskip::detection::insert_reference(intervals, last, 1, 5000));
    EXPECT_EQ(intervals[0].start_frame, 1); EXPECT_EQ(intervals[0].end_frame, 1001);
    ASSERT_TRUE(comskip::detection::insert_reference(intervals, last, 5000, 5000));
    EXPECT_EQ(intervals[2].start_frame, 4000); EXPECT_EQ(intervals[2].end_frame, 5000);
    ASSERT_TRUE(comskip::detection::insert_reference(intervals, last, 1250, 5000));
    EXPECT_EQ(intervals[1].start_frame, 1002); EXPECT_EQ(intervals[1].end_frame, 1499);
    EXPECT_FALSE(comskip::detection::insert_reference(intervals, last, 1250, 5000));
    EXPECT_FALSE(comskip::detection::insert_reference(intervals, last, 0, 5000));
    EXPECT_FALSE(comskip::detection::insert_reference(intervals, last, 5001, 5000));
    while (last >= 0) comskip::detection::erase_interval(intervals, last, 0);
    EXPECT_TRUE(intervals.empty()); EXPECT_EQ(last, -1);
}

TEST(CommercialStorage, ReviewInsertionGrowsBeyondFormerCapacityWithoutLosingNeighbors) {
    std::vector<Legacy_reffer_entry> intervals;
    for (int index = 0; index < 100000; ++index) intervals.push_back({index * 4 + 1, index * 4 + 2});
    int last = 99999;
    ASSERT_TRUE(comskip::detection::insert_reference(intervals, last, 199999, 400004));
    ASSERT_EQ(intervals.size(), 100001u); EXPECT_EQ(last, 100000);
    EXPECT_EQ(intervals[49999].start_frame, 199997);
    EXPECT_EQ(intervals[50000].start_frame, 199999); EXPECT_EQ(intervals[50000].end_frame, 200000);
    EXPECT_EQ(intervals[50001].start_frame, 200001);
    EXPECT_EQ(intervals.back().end_frame, 399998);
}

class LiveStorage : public ::testing::Test {
protected:
    std::filesystem::path directory;
    std::unique_ptr<RecordingContext> context;
    void SetUp() override {
        directory = std::filesystem::temp_directory_path() /
            ("comskip-live-storage-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        ASSERT_TRUE(std::filesystem::create_directory(directory));
        context = std::make_unique<RecordingContext>();
        context->settings.verbose = 0; context->settings.fps = 25;
        context->settings.commDetectMethod = BLACK_FRAME;
        context->settings.output_default = false; context->settings.output_edl = false;
        context->settings.output_live = true; context->settings.output_dvrmstb = false;
        context->settings.output_incommercial = true;
        context->settings.padding = 0;
        context->state.outbasename = (directory / "record").string();
        context->state.workbasename = context->state.outbasename;
        context->state.framenum_real = 10001;
        context->state.black.resize(1); context->state.black_count = 1;
    }
    void TearDown() override {
        context.reset(); std::error_code ignored; std::filesystem::remove_all(directory, ignored);
    }
    std::string read(const char* extension) {
        std::ifstream source(directory / (std::string("record") + extension));
        if (!source) throw std::runtime_error("Missing live storage output");
        return {std::istreambuf_iterator<char>(source), {}};
    }
    void complete_frames(int cause) {
        context->settings.shrink_logo = 1;
        context->settings.volume_slip = 1;
        context->settings.logo_threshold = 0.5;
        context->state.frame_count = 10000;
        context->state.frame.resize(10001);
        for (auto& frame : context->state.frame) {
            frame.logo_present = true;
            frame.currentGoodEdge = 1;
        }
        context->state.black_count = 3;
        context->state.black.resize(3);
        context->state.black[1].frame = 100; context->state.black[1].cause = cause;
        context->state.black[2].frame = 850; context->state.black[2].cause = cause;
    }
};
TEST_F(LiveStorage, EmptyCandidatesWriteZeroCommercialStatusAndReleaseFiles) {
    BuildCommListAsYouGo(*context);
    EXPECT_EQ(context->state.commercial_count, -1); EXPECT_TRUE(context->state.commercial.empty());
    EXPECT_EQ(context->state.reffer_count, -1); EXPECT_TRUE(context->state.reffer.empty());
    EXPECT_TRUE(read(".live").empty()); EXPECT_EQ(read(".incommercial"), "0\n");
    EXPECT_FALSE(context->state.live_file); EXPECT_FALSE(context->state.incommercial_file);
}
TEST_F(LiveStorage, InCommercialOutputWorksWithoutAnyCutlistOutputEnabled) {
    context->settings.output_live = false;
    BuildCommListAsYouGo(*context);
    EXPECT_EQ(context->state.commercial_count, -1);
    EXPECT_EQ(read(".incommercial"), "0\n");
    EXPECT_FALSE(std::filesystem::exists(directory / "record.live"));
    EXPECT_FALSE(context->state.incommercial_file);
}
TEST_F(LiveStorage, ActualCandidatesAndPublishedListsGrowBeyondFormerCapacity) {
    constexpr int runs = 100001;
    context->state.black.resize(runs * 2 + 1);
    context->state.black_count = runs * 2 + 1;
    for (int index = 0; index < runs; ++index) {
        auto& start = context->state.black[index * 2 + 1];
        auto& end = context->state.black[index * 2 + 2];
        start.frame = index * 5750 + 100; start.cause = C_b;
        end.frame = start.frame + 750; end.cause = C_b;
    }
    context->state.framenum_real = context->state.black.back().frame + 5001;
    BuildCommListAsYouGo(*context);
    ASSERT_EQ(context->state.commercial.size(), runs); EXPECT_EQ(context->state.commercial_count, runs - 1);
    ASSERT_EQ(context->state.reffer.size(), runs); EXPECT_EQ(context->state.reffer_count, runs - 1);
    for (int index = 0; index < runs; ++index) {
        ASSERT_EQ(context->state.commercial[index].start_frame, index * 5750 + 100);
        ASSERT_EQ(context->state.commercial[index].end_frame, index * 5750 + 850);
        ASSERT_EQ(context->state.reffer[index].start_frame, context->state.commercial[index].start_frame);
        ASSERT_EQ(context->state.reffer[index].end_frame, context->state.commercial[index].end_frame);
    }
    EXPECT_EQ(read(".incommercial"), "0\n");
}
TEST_F(LiveStorage, EnabledLogoExcludesPresentLogoAndDisabledLogoAcceptsSameBlackFrames) {
    complete_frames(C_b);
    context->settings.commDetectMethod |= LOGO;
    context->state.logoInfoAvailable = true;
    context->state.logo_block_count = 1;
    context->state.logo_block = {{1, 10000}};
    BuildCommListAsYouGo(*context);
    EXPECT_TRUE(context->state.commercial.empty()); EXPECT_TRUE(read(".live").empty());
    context->settings.commDetectMethod &= ~LOGO;
    context->state.lastFrameCommCalculated = 0;
    BuildCommListAsYouGo(*context);
    ASSERT_EQ(context->state.commercial.size(), 1u);
    EXPECT_EQ(context->state.commercial[0].start_frame, 100);
    EXPECT_EQ(context->state.commercial[0].end_frame, 850);
    EXPECT_TRUE(context->state.frame[100].logo_present);
}
TEST_F(LiveStorage, DisabledLogoLeavesSilenceCutpointBlackValidationInPlace) {
    complete_frames(C_v);
    BuildCommListAsYouGo(*context);
    EXPECT_TRUE(context->state.commercial.empty());
    context->state.frame[100].isblack = C_b;
    context->state.frame[850].isblack = C_b;
    context->state.lastFrameCommCalculated = 0;
    BuildCommListAsYouGo(*context);
    ASSERT_EQ(context->state.commercial.size(), 1u);
    EXPECT_EQ(context->state.commercial[0].start_frame, 100);
    EXPECT_EQ(context->state.commercial[0].end_frame, 850);
}
}
