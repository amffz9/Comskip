#include "recording_context.h"
#include "detection_methods.h"
#include "logo_detection.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>
namespace {
std::unique_ptr<RecordingContext> logo_context() {
    auto context=std::make_unique<RecordingContext>();
    context->state.width=context->state.videowidth=160;
    context->state.height=120;
    context->settings.edge_radius=2;
    context->settings.edge_step=1;
    context->settings.border=0;
    context->settings.edge_level_threshold=1;
    context->settings.aggressive_logo_rejection=1;
    context->settings.fps=25;
    context->state.ensure_pixel_buffers(true);
    context->state.frame.resize(102);
    context->state.logo_block.resize(2);
    return context;
}
TEST(LogoGeometryApplication, ExtremeEdgesFailBeforeDetectorWrites) {
    for(int mode=0;mode<3;++mode) {
        auto context=logo_context();
        std::vector<unsigned char> pixels(160u*120,7);
        context->state.hedge_count=91;
        if(mode==0) context->settings.edge_step=536870912;
        if(mode==1) context->settings.edge_radius=std::numeric_limits<int>::max();
        if(mode==2) context->settings.border=std::numeric_limits<int>::max();
        EXPECT_THROW(EdgeDetect(*context,pixels,0),std::invalid_argument);
        EXPECT_EQ(context->state.hedge_count,91);
        EXPECT_EQ(pixels,std::vector<unsigned char>(160u*120,7));
    }
}
TEST(LogoGeometryApplication, OrdinaryEdgesAreDetectedAndBuffersRequired) {
    auto context=logo_context();
    std::vector<unsigned char> pixels(160u*120);
    for(int y=0;y<120;++y) for(int x=0;x<160;++x) pixels[y*160+x]=(x%8<4 ? 0:255);
        EXPECT_NO_THROW(EdgeDetect(*context,pixels,0));
    EXPECT_TRUE(std::ranges::any_of(context->state.hor_edgecount,[](auto count){return count>0;}));
    context->state.hor_edgecount.clear();
        EXPECT_THROW(EdgeDetect(*context,pixels,0),std::invalid_argument);
}
TEST(LogoGeometryApplication, ShortAndExtremeFilterHistoryAreSafe) {
    auto context=logo_context();
    context->settings.logo_filter=1;
    for(auto& frame:context->state.frame) frame.logo_filter=8;
    EXPECT_NO_THROW(ProcessLogoTest(*context,1,0,0));
    EXPECT_EQ(context->state.frame[0].logo_filter,0);
    EXPECT_EQ(context->state.frame[1].logo_filter,0);
    EXPECT_EQ(context->state.frame[2].logo_filter,8);
    context->settings.logo_filter=1073741824;
    context->state.frame[25].logo_filter=7;
    EXPECT_THROW(ProcessLogoTest(*context,25,0,0),std::invalid_argument);
    EXPECT_EQ(context->state.frame[25].logo_filter,7);
}
TEST(LogoGeometryApplication, PersistedBoundsAreClippedBeforeNeighbourReads) {
    auto context=logo_context();
    context->state.clogoMinX=context->state.clogoMinY=-100;
    context->state.clogoMaxX=context->state.clogoMaxY=100000;
    std::vector<unsigned char> pixels(160u*120,7);
    std::ranges::fill(context->state.choriz_edgemask,1);
    std::ranges::fill(context->state.cvert_edgemask,1);
        EXPECT_NO_THROW(CheckStationLogoEdge(*context,pixels));
    EXPECT_EQ(context->state.currentGoodEdge,0);
}
TEST(LogoGeometryApplication, EmptyCaptionAndLogoReportsAreSafe) {
    auto context=logo_context();
    context->state.cc_block.clear();
    context->state.cc_block_count=0;
    context->state.framesprocessed=0;
    context->settings.fps=0;
    EXPECT_NO_THROW(PrintCCBlocks(*context));
    EXPECT_EQ(context->state.most_cc_type,comskip::detection::caption_type_value(comskip::detection::CaptionType::none));
    context->state.cc_block.resize(1);
    context->state.cc_block[0].start_frame=0;
    context->state.cc_block[0].end_frame=10;
    context->state.cc_block[0].type=comskip::detection::caption_type_value(comskip::detection::CaptionType::none);
    EXPECT_NO_THROW(PrintCCBlocks(*context));
    context->state.logo_block={{10,20}};
    context->state.logo_block_count=1;
    context->state.cblock.clear();
    context->state.block_count=0;
    EXPECT_NO_THROW(PrintLogoFrameGroups(*context));
}
TEST(LogoGeometryApplication, FrameStorageEndIsRejectedForBothLogoTransitions) {
    for (const bool closing : {false,true}) {
        auto context=logo_context();
        context->settings.logo_filter=0;
        context->state.logoFreq=1;
        context->state.minHitsForTrend=1;
        context->state.logo_block[0].start=0;
        context->state.lastLogoTest=closing;
        EXPECT_THROW(ProcessLogoTest(*context,static_cast<int>(context->state.frame.size()),
                                     closing ? 0 : 1,0),std::out_of_range);
    }
}
}
