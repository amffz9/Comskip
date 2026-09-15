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

static bool should_throw;
static void failing_scan(intptr_t index) {
    if (should_throw && index == 2) throw std::runtime_error("scan failure");
}
TEST(ScanWorkers, PropagatesTaskFailuresAndCanRunAgain) {
    ScanWorkers workers({failing_scan, failing_scan, failing_scan, failing_scan});
    should_throw = true;
    EXPECT_THROW(workers.run(), std::runtime_error);
    should_throw = false;
    EXPECT_NO_THROW(workers.run());
}

TEST(ScanWorkers, KeepsCapturedRecordingStateIndependent) {
    std::array<int, 4> first{}, second{};
    auto tasks = [](auto& values) {
        std::array<std::function<void(intptr_t)>, 4> result;
        for (auto& task : result) task = [&values](intptr_t index) { ++values[index]; };
        return result;
    };
    ScanWorkers first_workers(tasks(first)), second_workers(tasks(second));
    first_workers.run();
    first_workers.run();
    second_workers.run();
    for (unsigned index = 0; index < first.size(); ++index) {
        EXPECT_EQ(first[index], 2);
        EXPECT_EQ(second[index], 1);
    }
}
