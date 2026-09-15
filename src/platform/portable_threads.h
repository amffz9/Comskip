#ifndef COMSKIP_PORTABLE_THREADS_H
#define COMSKIP_PORTABLE_THREADS_H
#include <array>
#include <cstdint>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <exception>
#include <functional>

// Each run publishes a frame and waits for all four scans. jthread owns the
// worker lifetime; stop-aware waits allow clean shutdown, including exceptions
// during construction. Call run from one orchestration thread.
class ScanWorkers {
    std::mutex mutex_;
    std::condition_variable_any ready_;
    std::condition_variable finished_;
    std::size_t generation_ = 0;
    unsigned remaining_ = 0;
    std::exception_ptr failure_;
    std::array<std::jthread, 4> threads_;
public:
    explicit ScanWorkers(std::array<std::function<void(intptr_t)>, 4> tasks) {
        for (unsigned i = 0; i < threads_.size(); ++i) {
            threads_[i] = std::jthread([this, task = tasks[i], i](std::stop_token stop) {
                std::size_t seen = 0;
                std::unique_lock lock(mutex_);
                while (ready_.wait(lock, stop, [&] { return generation_ != seen; })) {
                    seen = generation_;
                    lock.unlock();
                    std::exception_ptr failure;
                    try { task(i); }
                    catch (...) { failure = std::current_exception(); }
                    lock.lock();
                    if (failure && !failure_) failure_ = failure;
                    if (--remaining_ == 0) finished_.notify_one();
                }
            });
        }
    }
    ScanWorkers(const ScanWorkers&) = delete;
    ScanWorkers& operator=(const ScanWorkers&) = delete;
    void run() {
        std::unique_lock<std::mutex> lock(mutex_);
        remaining_ = threads_.size();
        failure_ = nullptr;
        ++generation_;
        ready_.notify_all();
        finished_.wait(lock, [this] { return remaining_ == 0; });
        if (failure_) std::rethrow_exception(failure_);
    }
};
#endif
