#include "frame_mask.h"
#include <gtest/gtest.h>
#include <array>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using namespace comskip::detection;
TEST(FrameMask, ExactMasksPreservePaddingAndTrailingStorage) {
    std::vector<unsigned char> image(26, 9);
    apply_frame_mask(image, 4, 4, 6, {1, 1, 0, 0, 0, 1, 1});
    EXPECT_EQ(image, (std::vector<unsigned char>{
        0,0,0,0,9,9, 0,9,9,0,9,9, 0,9,9,0,9,9, 0,0,0,0,9,9, 9,9}));
}
TEST(FrameMask, RejectsEveryInvalidMaskBeforeAnyWrites) {
    for (int field = 0; field < 7; ++field) {
        for (int invalid : {-1, 101}) {
            FrameMaskOptions options{1,1,0,0,1,1,1};
            std::array<int*,7> fields{&options.bottom_rows,&options.top_rows,
                &options.bottom_percentage,&options.top_percentage,&options.side_columns,
                &options.left_columns,&options.right_columns};
            *fields[field] = invalid;
            std::vector<unsigned char> image(16, 7);
            EXPECT_THROW(apply_frame_mask(image, 4, 4, 4, options), std::invalid_argument)
                << field << ": " << invalid;
            EXPECT_EQ(image, std::vector<unsigned char>(16, 7));
        }
    }
}
TEST(FrameMask, RejectsGeometryAndShortSpansAtomically) {
    for (const auto& geometry : std::array<std::array<int,3>,5>{
             std::array{0,4,4}, {-1,4,4}, {4,0,4}, {4,-1,4}, {4,4,3}}) {
        std::vector<unsigned char> image(16, 5);
        EXPECT_THROW(apply_frame_mask(image, geometry[0], geometry[1], geometry[2], {}),
                     std::invalid_argument);
        EXPECT_EQ(image, std::vector<unsigned char>(16, 5));
    }
    std::vector<unsigned char> short_image(15, 5);
    EXPECT_THROW(apply_frame_mask(short_image,4,4,4,{1}),std::invalid_argument);
    EXPECT_EQ(short_image,std::vector<unsigned char>(15,5));
    EXPECT_THROW(apply_frame_mask({},std::numeric_limits<int>::max(),
        std::numeric_limits<int>::max(),std::numeric_limits<int>::max(),{}),std::invalid_argument);
}
TEST(FrameMask, PercentagesOverrideRowsWithoutChangingOptions) {
    const FrameMaskOptions options{1,1,50,25};
    std::vector<unsigned char> image(16,8);
    apply_frame_mask(image,4,4,4,options);
    EXPECT_EQ(image,(std::vector<unsigned char>{0,0,0,0,8,8,8,8,0,0,0,0,0,0,0,0}));
    EXPECT_EQ(options.bottom_rows,1);
    EXPECT_EQ(effective_mask_rows(1,50,6),3);
    EXPECT_EQ(effective_mask_rows(1,50,10),5);
    EXPECT_EQ(effective_mask_rows(1,0,10),1);
}
TEST(FrameMask, PercentageLimitsAndOverlappingMasks) {
    std::vector<unsigned char> image(12,6);
    apply_frame_mask(image,4,3,4,{0,0,100,0});
    EXPECT_EQ(image,std::vector<unsigned char>(12,0));
    image.assign(12,6);
    apply_frame_mask(image,4,3,4,{0,0,0,0,3,1,4});
    EXPECT_EQ(image,std::vector<unsigned char>(12,0));
    EXPECT_EQ(effective_mask_rows(0,100,std::numeric_limits<std::size_t>::max()),
              std::numeric_limits<std::size_t>::max());
}
TEST(FrameMask, NoMaskLeavesAllStorageUnchanged) {
    std::vector<unsigned char> image(20,4);
    apply_frame_mask(image,4,4,4,{});
    EXPECT_EQ(image,std::vector<unsigned char>(20,4));
}
}
