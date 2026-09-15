#include "buffer_growth.h"
#include <gtest/gtest.h>
#include <limits>
#include <memory>

namespace {
template<class T>
struct FailingAllocator {
    using value_type = T;
    std::shared_ptr<bool> fail;
    explicit FailingAllocator(std::shared_ptr<bool> flag) : fail(std::move(flag)) {}
    template<class U>
    FailingAllocator(const FailingAllocator<U>& other) : fail(other.fail) {}
    T* allocate(std::size_t count) {
        if (*fail) throw std::bad_alloc();
        return std::allocator<T>{}.allocate(count);
    }
    void deallocate(T* pointer, std::size_t count) noexcept {
        std::allocator<T>{}.deallocate(pointer, count);
    }
    template<class U>
    bool operator==(const FailingAllocator<U>& other) const { return fail == other.fail; }
};
struct Record {
    long frame{};
    int brightness{};
};
}

TEST(DetectionBufferGrowth, PreservesRecordsAndProvidesRequestedLookahead)
{
    std::vector<Record> buffer;
    long capacity = 0;
    ASSERT_TRUE(comskip::detection::grow_buffer(buffer, capacity, 19, 20, 2));
    ASSERT_EQ(capacity, 20);
    ASSERT_EQ(buffer.size(), 22);
    buffer[7] = {731, 87};
    ASSERT_TRUE(comskip::detection::grow_buffer(buffer, capacity, 74, 20, 2));
    EXPECT_EQ(capacity, 80);
    EXPECT_EQ(buffer.size(), 82);
    EXPECT_EQ(buffer[7].frame, 731);
    EXPECT_EQ(buffer[7].brightness, 87);
    EXPECT_EQ(buffer[74].brightness, 0);
    EXPECT_EQ(buffer[81].frame, 0);
    EXPECT_FALSE(comskip::detection::grow_buffer(buffer, capacity, 74, 20, 2));
}

TEST(DetectionBufferGrowth, FailedAllocationPreservesDataAndCapacity)
{
    auto fail = std::make_shared<bool>(false);
    std::vector<Record, FailingAllocator<Record>> buffer{FailingAllocator<Record>{fail}};
    long capacity = 0;
    comskip::detection::grow_buffer(buffer, capacity, 4, 5, 2);
    buffer[4] = {94, 65};
    const auto* original = buffer.data();
    *fail = true;
    EXPECT_THROW(comskip::detection::grow_buffer(buffer, capacity, 1000, 5, 2), std::bad_alloc);
    EXPECT_EQ(capacity, 5);
    EXPECT_EQ(buffer.size(), 7);
    EXPECT_EQ(buffer.data(), original);
    EXPECT_EQ(buffer[4].frame, 94);
    EXPECT_EQ(buffer[4].brightness, 65);
    *fail = false;
    EXPECT_TRUE(comskip::detection::grow_buffer(buffer, capacity, 1000, 5, 2));
    EXPECT_EQ(buffer[4].frame, 94);
}

TEST(DetectionBufferGrowth, InvalidOrOverflowingRequestDoesNotChangeStorage)
{
    std::vector<Record> buffer(4);
    buffer[0] = {14, 44};
    long capacity = 3;
    EXPECT_THROW(comskip::detection::grow_buffer(buffer, capacity, -1, 20, 2), std::out_of_range);
    EXPECT_THROW(comskip::detection::grow_buffer(buffer, capacity,
        std::numeric_limits<long>::max(), 20, 2), std::length_error);
    EXPECT_EQ(capacity, 3);
    EXPECT_EQ(buffer.size(), 4);
    EXPECT_EQ(buffer[0].brightness, 44);
}
