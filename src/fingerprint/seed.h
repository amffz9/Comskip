#pragma once
#include "fingerprint/coder.h"
#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace comskip::fingerprint {
// Fills the buffer from the audio stream; returns the bytes read, 0 at its end.
using ReadBytes = std::function<std::size_t(std::span<std::byte>)>;

struct SeedResult {
    std::vector<CodeRun> runs;  // one per R message, in stream order
    bool ended{};               // the E message arrived
    long long codes() const;
};

// Reads the audio stream to its end and codes every run with its own Coder.
// Throws InputInvalid for a malformed stream. Audio cut off inside a message is
// dropped, as when the sender stopped early.
SeedResult seed(const ReadBytes& read);

// The code stream as one JSON object: the recording, the code period and each
// run's start (capture seconds) and codes (unsigned 32-bit). Until the Library
// (step F4) stores streams, this file is what `seed` leaves.
std::string code_stream_json(std::string_view recording, const SeedResult& result);
// The pass result of a seed: no marks files and nothing promoted.
std::string seed_result_json(std::string_view recording, const SeedResult& result);
}
