#include "analysis_policy.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace comskip::platform {
ScopedAnalysisPolicy::ScopedAnalysisPolicy(bool background_priority) noexcept
{
#ifdef _WIN32
    if (background_priority) {
        previous_process_priority_ = GetPriorityClass(GetCurrentProcess());
        if (previous_process_priority_ && previous_process_priority_ != IDLE_PRIORITY_CLASS)
            process_priority_changed_ = SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS) != 0;
        previous_thread_priority_ = GetThreadPriority(GetCurrentThread());
        if (previous_thread_priority_ != THREAD_PRIORITY_ERROR_RETURN
            && previous_thread_priority_ != THREAD_PRIORITY_HIGHEST)
            thread_priority_changed_ = SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST) != 0;
    }
    previous_execution_state_ = SetThreadExecutionState(
        ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED);
#else
    (void)background_priority;
#endif
}

ScopedAnalysisPolicy::~ScopedAnalysisPolicy() noexcept
{
#ifdef _WIN32
    if (previous_execution_state_)
        SetThreadExecutionState(previous_execution_state_ | ES_CONTINUOUS);
    if (thread_priority_changed_ && GetThreadPriority(GetCurrentThread()) == THREAD_PRIORITY_HIGHEST)
        SetThreadPriority(GetCurrentThread(), previous_thread_priority_);
    // Process priority is shared. A nested scope that found IDLE did not own a
    // change, and must not restore IDLE after an outer analysis restores normal.
    // Avoid overwriting an unrelated priority change made while processing.
    if (process_priority_changed_ && GetPriorityClass(GetCurrentProcess()) == IDLE_PRIORITY_CLASS)
        SetPriorityClass(GetCurrentProcess(), previous_process_priority_);
#endif
}
}
