#include "checked_format.h"
#include <gtest/gtest.h>
TEST(CheckedFormat, RejectsLongAndCombinedPathsWithoutWritingPastBuffer) {
    struct { char path[8]; char guard = 'G'; } storage{};
    EXPECT_THROW(comskip::checked_format(storage.path, "%s", "12345678"), std::length_error);
    EXPECT_EQ(storage.guard, 'G');
    EXPECT_THROW(comskip::checked_format(storage.path, "%s/%s", "1234", "5678"), std::length_error);
    EXPECT_EQ(storage.guard, 'G');
    comskip::checked_format(storage.path, "%s", "1234567");
    EXPECT_STREQ(storage.path, "1234567");
}
