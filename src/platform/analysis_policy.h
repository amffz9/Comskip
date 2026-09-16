#ifndef COMSKIP_PLATFORM_ANALYSIS_POLICY_H
#define COMSKIP_PLATFORM_ANALYSIS_POLICY_H
#include <cstdint>

namespace comskip::platform {
// Best-effort scheduling and sleep prevention for one analysis. Construct and
// destroy on the same thread; native thread execution requirements are local
// to that thread. Unsupported platforms use the standard scheduling defaults.
class ScopedAnalysisPolicy {
    std::uint32_t previous_process_priority_{};
    std::uint32_t previous_execution_state_{};
    int previous_thread_priority_{};
    bool process_priority_changed_{};
    bool thread_priority_changed_{};
public:
    explicit ScopedAnalysisPolicy(bool background_priority = true) noexcept;
    ~ScopedAnalysisPolicy() noexcept;
    ScopedAnalysisPolicy(const ScopedAnalysisPolicy&) = delete;
    ScopedAnalysisPolicy& operator=(const ScopedAnalysisPolicy&) = delete;
    ScopedAnalysisPolicy(ScopedAnalysisPolicy&&) = delete;
    ScopedAnalysisPolicy& operator=(ScopedAnalysisPolicy&&) = delete;
};
}
#endif
