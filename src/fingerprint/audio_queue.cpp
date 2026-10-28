#include "fingerprint/audio_queue.h"
#include "fingerprint/coder.h"
#include <algorithm>

namespace comskip::fingerprint {
void AudioQueue::push(AudioPiece piece) {
    queued_ += piece.samples.size();
    peak_ = std::max(peak_, queued_);
    pieces_.push_back(std::move(piece));
    ready_.notify_one();
}

void AudioQueue::run_start(double start) {
    const std::lock_guard lock(mutex_);
    run_start_ = start;
    run_samples_ = 0;
    gap_ = false;
    push({AudioPiece::Kind::run, start, {}});
}

void AudioQueue::audio(std::span<const std::int16_t> samples) {
    const std::lock_guard lock(mutex_);
    const auto at = run_samples_;
    run_samples_ += static_cast<long long>(samples.size());
    if (queued_ + samples.size() > capacity_) {
        dropped_ += static_cast<long long>(samples.size());
        gap_ = true;
        return;
    }
    if (gap_) {
        push({AudioPiece::Kind::run, run_start_ + static_cast<double>(at) / sample_rate, {}});
        gap_ = false;
    }
    push({AudioPiece::Kind::audio, 0, {samples.begin(), samples.end()}});
}

void AudioQueue::end() {
    const std::lock_guard lock(mutex_);
    push({AudioPiece::Kind::end, 0, {}});
}

void AudioQueue::close(std::exception_ptr error) {
    const std::lock_guard lock(mutex_);
    closed_ = true;
    error_ = error;
    ready_.notify_all();
}

std::optional<AudioPiece> AudioQueue::pop(std::chrono::milliseconds wait) {
    std::unique_lock lock(mutex_);
    if (!ready_.wait_for(lock, wait, [&] { return !pieces_.empty() || closed_; }) || pieces_.empty()) return std::nullopt;
    auto piece = std::move(pieces_.front());
    pieces_.pop_front();
    queued_ -= piece.samples.size();
    return piece;
}

bool AudioQueue::drained() const {
    const std::lock_guard lock(mutex_);
    return closed_ && pieces_.empty();
}

std::exception_ptr AudioQueue::error() const {
    const std::lock_guard lock(mutex_);
    return error_;
}

long long AudioQueue::dropped_samples() const {
    const std::lock_guard lock(mutex_);
    return dropped_;
}

std::size_t AudioQueue::peak_samples() const {
    const std::lock_guard lock(mutex_);
    return peak_;
}
}
