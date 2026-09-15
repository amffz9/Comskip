#include "portable_threads.h"
#include <gtest/gtest.h>
#include <array>
static std::array<int, 4> counts{};
static int published;
static std::array<int, 4> observed{};
static void scan(intptr_t index) { ++counts[index]; observed[index] = published; }
TEST(ScanWorkers, PublishesFramesWaitsForResultsAndJoinsAtShutdown) {
    counts.fill(0);
    for (int lifetime = 0; lifetime < 3; ++lifetime) {
        ScanWorkers workers({scan, scan, scan, scan});
        for (int frame = 1; frame <= 100; ++frame) {
            published = frame;
            workers.run();
            for (unsigned i = 0; i < counts.size(); ++i) {
                EXPECT_EQ(counts[i], lifetime * 100 + frame);
                EXPECT_EQ(observed[i], frame);
            }
        }
    }
}
