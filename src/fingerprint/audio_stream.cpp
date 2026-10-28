#include "fingerprint/audio_stream.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <string>

namespace comskip::fingerprint {
namespace {
constexpr std::size_t header_size = 5;

std::uint64_t little_endian(std::span<const std::byte> bytes) {
    std::uint64_t value = 0;
    for (std::size_t index = bytes.size(); index-- > 0;)
        value = value << 8 | std::to_integer<std::uint64_t>(bytes[index]);
    return value;
}
}

void AudioStreamParser::push(std::span<const std::byte> bytes) {
    while (!bytes.empty()) {
        if (ended_) throw InputInvalid("Audio stream continues after its end message");
        if (header_.size() < header_size) {
            const auto take = std::min(bytes.size(), header_size - header_.size());
            header_.insert(header_.end(), bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(take));
            bytes = bytes.subspan(take);
            if (header_.size() < header_size) return;
            const auto type = static_cast<char>(header_[0]);
            payload_size_ = static_cast<std::uint32_t>(little_endian(std::span(header_).subspan(1)));
            const bool valid = (type == 'R' && payload_size_ == 8) || (type == 'E' && payload_size_ == 0) ||
                               (type == 'A' && payload_size_ % 2 == 0 && payload_size_ <= max_payload);
            if (!valid)
                throw InputInvalid("Audio stream message '" + std::string(1, type) + "' has an invalid type or length " +
                                   std::to_string(payload_size_));
            if (type == 'A' && !in_run_) throw InputInvalid("Audio stream has audio before its first run start");
            payload_.reserve(payload_size_);
        }
        const auto take = std::min<std::size_t>(bytes.size(), payload_size_ - payload_.size());
        payload_.insert(payload_.end(), bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(take));
        bytes = bytes.subspan(take);
        if (payload_.size() == payload_size_) complete();
    }
}

void AudioStreamParser::complete() {
    const auto type = static_cast<char>(header_[0]);
    if (type == 'R') {
        const auto start = std::bit_cast<double>(little_endian(payload_));
        if (!std::isfinite(start) || start < 0) throw InputInvalid("Audio stream run start is negative or not finite");
        in_run_ = true;
        events_.run_start(start);
    } else if (type == 'A') {
        samples_.resize(payload_.size() / 2);
        for (std::size_t index = 0; index < samples_.size(); ++index)
            samples_[index] = static_cast<std::int16_t>(std::to_integer<unsigned>(payload_[2 * index]) |
                                                        std::to_integer<unsigned>(payload_[2 * index + 1]) << 8);
        events_.audio(samples_);
    } else {
        ended_ = true;
        events_.end();
    }
    header_.clear();
    payload_.clear();
}
}
