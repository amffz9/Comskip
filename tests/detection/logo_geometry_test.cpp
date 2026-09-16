#include "logo_geometry.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
using namespace comskip::detection;
TEST(LogoGeometry, PreservesOrdinarySamplingAndMiddleSkip) {
    const auto geometry=validate_logo_scan(60,60,64,{1,1,1});
    std::vector<int> expected;
    for(int x=6;x<=20;++x) expected.push_back(x);
    for(int x=40;x<54;++x) expected.push_back(x);
    EXPECT_EQ(geometry.columns,expected);
    EXPECT_EQ(geometry.rows,expected);
    EXPECT_EQ(geometry.expansion,4);
    EXPECT_EQ(geometry.storage_size,64u*60);
}
TEST(LogoGeometry, RejectsExtremeAndInvalidOptionsBeforeAllocation) {
    for(auto options:std::array<LogoScanOptions,7>{LogoScanOptions{0,1,0},
        {-1,1,0},{1,0,0},{1,-1,0},{1,1,-1},
        {std::numeric_limits<int>::max(),1,10},{1,536870912,0}})
        EXPECT_THROW(validate_logo_scan(320,240,320,options),std::invalid_argument);
    EXPECT_THROW(validate_logo_scan(320,240,319,{1,1,0}),std::invalid_argument);
    EXPECT_THROW(validate_logo_scan(0,240,320,{1,1,0}),std::invalid_argument);
    EXPECT_THROW(validate_logo_scan(320,-1,320,{1,1,0}),std::invalid_argument);
    EXPECT_THROW(validate_logo_scan(std::numeric_limits<int>::max(),240,
        std::numeric_limits<int>::max(),{1,1,0}),std::invalid_argument);
}
TEST(LogoGeometry, RestrictionsAndHaloStayInsideVisibleImage) {
    const auto geometry=validate_logo_scan(160,120,192,{2,2,0,true,true,false});
    for(int x:geometry.columns) {EXPECT_GE(x,80);EXPECT_LT(x+3,160);EXPECT_GE(x-3,0);}
    for(int y:geometry.rows) {EXPECT_GE(y,60);EXPECT_LT(y+3,120);EXPECT_GE(y-3,0);}
    EXPECT_TRUE(validate_logo_scan(160,120,192,{2,2,0,true,true,true}).rows.empty());
}
TEST(LogoFilterHistory, ClipsShortHistoryAndPreservesCompleteWindows) {
    auto short_history=validate_logo_filter(1,25,1,2);
    EXPECT_EQ(short_history.recent_begin,0);
    EXPECT_EQ(short_history.delayed_frame,1);
    EXPECT_FALSE(short_history.complete_windows);
    auto history=validate_logo_filter(2,25,101,102);
    EXPECT_EQ(history.delay,50);
    EXPECT_EQ(history.recent_begin,77);
    EXPECT_EQ(history.delayed_frame,51);
    EXPECT_TRUE(history.complete_windows);
    EXPECT_FALSE(validate_logo_filter(2,25,100,101).complete_windows);
}
TEST(LogoFilterHistory, RejectsUnrepresentableProductsAndInvalidFrameBounds) {
    EXPECT_THROW(validate_logo_filter(1073741824,25,25,26),std::invalid_argument);
    EXPECT_THROW(validate_logo_filter(1,std::numeric_limits<int>::max(),1,2),std::invalid_argument);
    EXPECT_THROW(validate_logo_filter(-1,25,1,2),std::invalid_argument);
    EXPECT_THROW(validate_logo_filter(1,0,1,2),std::invalid_argument);
    EXPECT_THROW(validate_logo_filter(1,25,-1,2),std::invalid_argument);
    EXPECT_THROW(validate_logo_filter(1,25,2,2),std::invalid_argument);
}
