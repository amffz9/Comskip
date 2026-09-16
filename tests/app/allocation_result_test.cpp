#include "allocation_result.h"
#include <gtest/gtest.h>
#include <stdexcept>

TEST(AllocationResult, ReportsBadAllocAndPreservesOtherExceptions) {
    EXPECT_TRUE(comskip::attempt_allocation([]{}));
    const auto failed=comskip::attempt_allocation([]{ throw std::bad_alloc{}; });
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error(),comskip::AllocationError::insufficient_memory);
    EXPECT_THROW(comskip::attempt_allocation([]{ throw std::runtime_error("operation"); }),std::runtime_error);
}
