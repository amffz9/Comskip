#pragma once
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace comskip::fingerprint {
// Chromaprint's own rate: audio at this rate is not resampled again.
inline constexpr int sample_rate = 11025;
// One code per 1365 samples (4096-sample frames with two thirds overlap), so the
// code period is 1365/11025 s = 0.123810 s. Rounding it to 0.1239 s drifts by
// about 3.9 s over 90 minutes.
inline constexpr int code_samples = 1365;
inline constexpr double code_period = static_cast<double>(code_samples) / sample_rate;
// Code i covers samples [i * code_samples, i * code_samples + code_span_samples):
// 20 frames, 5 for the chroma filter and 16 for the classifiers sharing one. A
// run shorter than this has no codes.
inline constexpr int code_span_samples = 19 * code_samples + 4096;

// Codes of one run: code i starts at start + i * code_period.
struct CodeRun {
    double start{};
    std::vector<std::uint32_t> codes;
};

// Codes one run of 16-bit mono audio at 11025 Hz as it arrives. A single
// Chromaprint context lives for the whole run and is never restarted; each feed
// returns every code whose audio is now complete, so the codes of a run fed in
// pieces equal those of the run fed at once. Throws runtime_error when
// Chromaprint fails.
class Coder {
public:
    Coder();
    ~Coder();
    Coder(Coder&&) noexcept;
    Coder& operator=(Coder&&) noexcept;
    std::vector<std::uint32_t> feed(std::span<const std::int16_t> samples);
    long long samples_fed() const { return samples_fed_; }
    long long codes_taken() const { return codes_taken_; }

private:
    struct Context;
    std::unique_ptr<Context> context_;
    long long samples_fed_{};
    long long codes_taken_{};
};

// The whole run at once.
CodeRun code(double start, std::span<const std::int16_t> samples);
}
