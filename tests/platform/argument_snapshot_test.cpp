#include "arguments.h"
#include <gtest/gtest.h>

TEST(ArgumentSnapshot, OwnsIndependentLongUtf8Arguments) {
    std::string program = "comskip";
    std::string recording = "録画/" + std::string(5000, 'x') + " clip.ts";
    char* values[] = {program.data(), recording.data()};
    const auto saved = comskip::snapshot_arguments(2, values);
    const auto expected = recording;
    program[0] = 'X';
    recording.clear();
    ASSERT_EQ(saved.size(), 2u);
    EXPECT_EQ(saved[0], "comskip");
    EXPECT_EQ(saved[1], expected);
    EXPECT_TRUE(comskip::snapshot_arguments(0, nullptr).empty());
}
TEST(ArgumentSnapshot, RejectsInvalidArrays) {
    char* values[] = {nullptr};
    EXPECT_THROW(comskip::snapshot_arguments(-1, values), std::invalid_argument);
    EXPECT_THROW(comskip::snapshot_arguments(1, nullptr), std::invalid_argument);
    EXPECT_THROW(comskip::snapshot_arguments(1, values), std::invalid_argument);
}
