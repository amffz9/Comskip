#include "media/input_position.h"
#include <gtest/gtest.h>

namespace {

struct FakeIoContext {};
struct FakeFormatContext {
    FakeIoContext* pb{};
};

} // namespace

TEST(InputPosition, KeepsPreviousPositionWithoutFormatContext) {
    EXPECT_EQ(comskip::media::input_position<FakeFormatContext>(nullptr, 42,
        [](FakeIoContext*) { return 100; }), 42);
}

TEST(InputPosition, KeepsPreviousPositionWithoutIoContext) {
    const FakeFormatContext format_context{};
    EXPECT_EQ(comskip::media::input_position(&format_context, 42,
        [](FakeIoContext*) { return 100; }), 42);
}

TEST(InputPosition, ObtainsPositionFromAvailableIoContext) {
    FakeIoContext io_context;
    const FakeFormatContext format_context{&io_context};
    EXPECT_EQ(comskip::media::input_position(&format_context, 42,
        [](FakeIoContext* input) { return input == nullptr ? -1 : 100; }), 100);
}
