#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace comskip::fingerprint {
// The stream breaks the audio stream format (docs/fingerprint-plan.md, "Cross-repo
// formats"); the tool exits 2.
struct InputInvalid : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class AudioStreamEvents {
public:
    virtual ~AudioStreamEvents() = default;
    // An R message: a new run whose first sample is at start capture seconds.
    virtual void run_start(double start) = 0;
    // An A message: samples that follow the run's earlier samples.
    virtual void audio(std::span<const std::int16_t> samples) = 0;
    // The E message: the recording's audio is complete.
    virtual void end() = 0;
};

// Splits the bytes of the audio stream into messages as they arrive, in pieces
// of any size: a 1-byte type, a 4-byte little-endian payload length, then the
// payload. Throws InputInvalid for an unknown type, a payload of the wrong size,
// audio before the first R, a run start that is negative or not finite, or any
// byte after E.
class AudioStreamParser {
public:
    // Larger A payloads are rejected rather than buffered (about 95 s of audio).
    static constexpr std::uint32_t max_payload = 1u << 21;

    explicit AudioStreamParser(AudioStreamEvents& events) : events_(events) {}
    void push(std::span<const std::byte> bytes);
    bool ended() const { return ended_; }
    // Bytes of a message not yet complete. At end of input they are discarded:
    // the audio stopped early.
    std::size_t pending() const { return header_.size() + payload_.size(); }

private:
    void complete();

    AudioStreamEvents& events_;
    std::vector<std::byte> header_;
    std::vector<std::byte> payload_;
    std::uint32_t payload_size_{};
    std::vector<std::int16_t> samples_;
    bool in_run_{};
    bool ended_{};
};
}
