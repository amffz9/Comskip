#pragma once
#include "fingerprint/audio_stream.h"
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <exception>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

namespace comskip::fingerprint {
// One message of the audio stream as the reading thread hands it on.
struct AudioPiece {
    enum class Kind { run, audio, end };
    Kind kind{};
    double start{};                     // run: the capture second of its first sample
    std::vector<std::int16_t> samples;  // audio
};

// The live job's input (docs/fingerprint-plan.md, FingerprintTool.live): the
// thread reading standard input pushes, the matching thread pops. Pushing never
// waits for the matching thread. Audio that would take the queue past its
// capacity is dropped whole and counted, and the next audio kept starts a new run
// at the capture second it belongs to, so nothing is matched across the gap.
// Run starts and the end are never dropped.
class AudioQueue final : public AudioStreamEvents {
public:
    explicit AudioQueue(std::size_t capacity_samples) : capacity_(capacity_samples) {}

    void run_start(double start) override;
    void audio(std::span<const std::int16_t> samples) override;
    void end() override;
    // No more input: standard input ended, or reading it failed with error.
    void close(std::exception_ptr error = nullptr);

    // The next piece, waiting up to `wait` for one; nullopt when none came, and
    // for good once the queue is closed and empty.
    std::optional<AudioPiece> pop(std::chrono::milliseconds wait);
    bool drained() const;
    std::exception_ptr error() const;
    long long dropped_samples() const;
    // The most audio ever waiting at once.
    std::size_t peak_samples() const;

private:
    void push(AudioPiece piece);

    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<AudioPiece> pieces_;
    std::size_t queued_{};
    std::size_t peak_{};
    long long dropped_{};
    bool closed_{};
    std::exception_ptr error_;
    // The run being read: its start and every sample of it seen, kept or not.
    double run_start_{};
    long long run_samples_{};
    bool gap_{};  // audio of this run was dropped since the last piece kept
};
}
