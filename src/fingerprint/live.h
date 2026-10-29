#pragma once
#include "fingerprint/inputs.h"
#include "fingerprint/matching.h"
#include "fingerprint/seed.h"
#include <string>
#include <string_view>

// FingerprintTool.live (docs/fingerprint-plan.md, "PART: FingerprintTool"): one
// live capture. A thread reads the audio stream into a bounded queue, the
// matching thread codes and matches it and keeps the MarksFile, and a store
// thread appends the codes and refreshes the live index every 10 s.
namespace comskip::fingerprint {
struct LiveOptions {
    std::string store;
    std::string recording;
    Context context;
    std::string evidence;  // the live Evidence file; it may not exist yet
    std::string marks;     // the live MarksFile
    std::string log;       // optional: one JSON line per MarksFile and match event, for measurement
    bool intro_credits{};
    // Tests: once this capture second is reached, the matching thread sleeps
    // stall_for seconds, as a starved process would.
    double stall_at = -1;
    double stall_for{};
    MatchRules matching;
};

struct LiveResult {
    bool ended{};
    long long codes{};
    bool marks_written{};
    long long matches{}, withdrawn{};
    double dropped_seconds{};
};

// Audio the matching thread falls this far behind on is dropped and counted.
inline constexpr double live_queue_seconds = 30;
// The live index in memory holds at most this many bytes of codes (twice that
// with its lookup).
inline constexpr std::size_t live_index_code_bytes = 10u << 20;

// Runs the job to the end of the audio. Throws InputInvalid for a malformed
// stream and the store errors when the store can't be used at all; a store busy
// past its wait is reported on standard error and retried 10 s later.
LiveResult live(const LiveOptions& options, const ReadBytes& read);
std::string live_result_json(std::string_view recording, std::string_view marks_path, const LiveResult& result);
}
