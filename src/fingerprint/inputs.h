#pragma once
#include "fingerprint/json.h"
#include <optional>
#include <string>
#include <vector>

// The tool's inputs from the TV app other than audio (docs/fingerprint-plan.md,
// "Cross-repo formats" and "Part contracts"). Times are capture seconds.
namespace comskip::fingerprint {
struct Seam {
    double at{};
    std::vector<std::string> kinds;  // black, silence, scene, aspect
};
struct Interval {
    double from{};
    double to{};
};
struct Block {
    double from{};
    double to{};
    bool commercial{};  // decision "commercial"; otherwise "programme"
};

// Comskip's evidence for one capture, mapped to capture seconds.
struct Evidence {
    std::string capture_id;
    long long revision{};
    bool final{};
    bool logo_known{};
    std::vector<Seam> seams;     // sorted by time
    std::vector<Interval> logo;  // sorted by start
    std::vector<Block> blocks;   // sorted by start
};

struct Programme {
    double from{};
    double to{};
    std::string title;
    std::string series_key;
};

// What the app knows about a recording when it hands the tool its audio.
struct Context {
    std::string channel;          // empty when unknown
    double air_start{};           // Unix seconds of capture second 0
    std::vector<Programme> programmes;
    std::string source = "capture";  // "capture" or "plex"
};

// Both throw json::Invalid for a document that breaks the format.
Evidence evidence_from_json(const json::Value& document);
Context context_from_json(const json::Value& document);
// The same objects as JSON, as the store keeps them.
std::string evidence_to_json(const Evidence& evidence);
std::string context_to_json(const Context& context);

// The programme whose window holds the time, or nullptr.
const Programme* programme_at(const Context& context, double time);
}
