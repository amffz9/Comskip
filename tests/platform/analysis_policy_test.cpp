#include "analysis_policy.h"
#include <gtest/gtest.h>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {
class ExecutionRequirements {
    EXECUTION_STATE previous_ = SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED);
public:
    ~ExecutionRequirements() { if (previous_) SetThreadExecutionState(previous_ | ES_CONTINUOUS); }
    bool valid() const { return previous_ != 0; }
};
EXECUTION_STATE execution_state() {
    const auto previous = SetThreadExecutionState(ES_CONTINUOUS);
    if (previous) SetThreadExecutionState(previous | ES_CONTINUOUS);
    return previous;
}
}

TEST(AnalysisPolicy, RestoresPreviousStateWhenAnExceptionUnwinds)
{
    const auto process_priority = GetPriorityClass(GetCurrentProcess());
    const auto thread_priority = GetThreadPriority(GetCurrentThread());
    ASSERT_NE(process_priority, 0u);
    ASSERT_NE(thread_priority, THREAD_PRIORITY_ERROR_RETURN);
    ExecutionRequirements requirements;
    ASSERT_TRUE(requirements.valid());
    const auto execution = execution_state();
    EXPECT_THROW({
        comskip::platform::ScopedAnalysisPolicy policy;
        EXPECT_EQ(GetPriorityClass(GetCurrentProcess()), static_cast<DWORD>(IDLE_PRIORITY_CLASS));
        EXPECT_EQ(GetThreadPriority(GetCurrentThread()), THREAD_PRIORITY_HIGHEST);
        throw std::runtime_error("analysis failed");
    }, std::runtime_error);
    EXPECT_EQ(GetPriorityClass(GetCurrentProcess()), process_priority);
    EXPECT_EQ(GetThreadPriority(GetCurrentThread()), thread_priority);
    EXPECT_EQ(execution_state(), execution);
}

TEST(AnalysisPolicy, NestedAnalysesRestoreTheOuterAndOriginalState)
{
    const auto process_priority = GetPriorityClass(GetCurrentProcess());
    const auto thread_priority = GetThreadPriority(GetCurrentThread());
    ExecutionRequirements requirements;
    ASSERT_TRUE(requirements.valid());
    const auto execution = execution_state();
    {
        comskip::platform::ScopedAnalysisPolicy outer;
        const auto outer_execution = execution_state();
        {
            comskip::platform::ScopedAnalysisPolicy inner;
        }
        EXPECT_EQ(GetPriorityClass(GetCurrentProcess()), static_cast<DWORD>(IDLE_PRIORITY_CLASS));
        EXPECT_EQ(GetThreadPriority(GetCurrentThread()), THREAD_PRIORITY_HIGHEST);
        EXPECT_EQ(execution_state(), outer_execution);
    }
    EXPECT_EQ(GetPriorityClass(GetCurrentProcess()), process_priority);
    EXPECT_EQ(GetThreadPriority(GetCurrentThread()), thread_priority);
    EXPECT_EQ(execution_state(), execution);
}

TEST(AnalysisPolicy, GuiModePreservesSchedulingAndRestoresPowerOnReturn)
{
    const auto process_priority = GetPriorityClass(GetCurrentProcess());
    const auto thread_priority = GetThreadPriority(GetCurrentThread());
    ExecutionRequirements requirements;
    ASSERT_TRUE(requirements.valid());
    const auto execution = execution_state();
    const auto analyze = [&] {
        comskip::platform::ScopedAnalysisPolicy policy(false);
        EXPECT_EQ(GetPriorityClass(GetCurrentProcess()), process_priority);
        EXPECT_EQ(GetThreadPriority(GetCurrentThread()), thread_priority);
        return 17;
    };
    EXPECT_EQ(analyze(), 17);
    EXPECT_EQ(execution_state(), execution);
}
#endif
