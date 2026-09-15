#pragma once
#include <exception>

namespace comskip {
// Propagates an already reported error, help request, or user cancellation to
// the application boundary while allowing automatic resources to unwind.
class ExitRequested final : public std::exception {
    int status_;
public:
    explicit ExitRequested(int status) noexcept : status_(status) {}
    int status() const noexcept { return status_; }
    const char* what() const noexcept override { return "Application exit requested"; }
};
[[noreturn]] inline void request_exit(int status) { throw ExitRequested(status); }
}
