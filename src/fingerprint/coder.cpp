#include "fingerprint/coder.h"
#include <chromaprint.h>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace comskip::fingerprint {
struct Coder::Context {
    ChromaprintContext* chromaprint{};
    ~Context() { chromaprint_free(chromaprint); }
};

Coder::Coder() : context_(std::make_unique<Context>()) {
    context_->chromaprint = chromaprint_new(CHROMAPRINT_ALGORITHM_DEFAULT);
    if (!context_->chromaprint) throw std::runtime_error("Chromaprint context cannot be created");
    if (!chromaprint_start(context_->chromaprint, sample_rate, 1))
        throw std::runtime_error("Chromaprint cannot start at 11025 Hz mono");
    if (chromaprint_get_item_duration(context_->chromaprint) != code_samples)
        throw std::runtime_error("Chromaprint's code period differs from the one this tool assumes");
}

Coder::~Coder() = default;
Coder::Coder(Coder&&) noexcept = default;
Coder& Coder::operator=(Coder&&) noexcept = default;

std::vector<std::uint32_t> Coder::feed(std::span<const std::int16_t> samples) {
    auto* const chromaprint = context_->chromaprint;
    while (!samples.empty()) {
        const auto piece = std::min<std::size_t>(samples.size(), std::numeric_limits<int>::max());
        if (!chromaprint_feed(chromaprint, samples.data(), static_cast<int>(piece)))
            throw std::runtime_error("Chromaprint rejected audio");
        samples = samples.subspan(piece);
        samples_fed_ += static_cast<long long>(piece);
    }
    // Chromaprint holds up to 32768 samples before analysing them. Finishing
    // only hands those on (nothing is resampled at 11025 Hz), so the run goes on
    // with the same context; the codes taken are then cleared, not recomputed.
    if (!chromaprint_finish(chromaprint)) throw std::runtime_error("Chromaprint cannot flush audio");
    int size = 0;
    if (!chromaprint_get_raw_fingerprint_size(chromaprint, &size))
        throw std::runtime_error("Chromaprint cannot report its codes");
    std::vector<std::uint32_t> codes;
    if (size <= 0) return codes;
    std::uint32_t* raw = nullptr;
    if (!chromaprint_get_raw_fingerprint(chromaprint, &raw, &size) || !raw)
        throw std::runtime_error("Chromaprint cannot return its codes");
    codes.assign(raw, raw + size);
    chromaprint_dealloc(raw);
    if (!chromaprint_clear_fingerprint(chromaprint)) throw std::runtime_error("Chromaprint cannot clear its codes");
    codes_taken_ += size;
    return codes;
}

CodeRun code(double start, std::span<const std::int16_t> samples) {
    Coder coder;
    return {start, coder.feed(samples)};
}
}
