#pragma once
#include "fingerprint/inputs.h"
#include "fingerprint/library.h"
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Combiner (docs/fingerprint-plan.md, "PART: Combiner"): matches plus Evidence
// make marks with kind and state. Pure: the same inputs give the same marks.
namespace comskip::fingerprint {
struct EvidenceMissing : std::runtime_error {
    using std::runtime_error::runtime_error;
};

enum class MarkKind { ad, in_programme_ad, intro, credits };
enum class MarkState { provisional, settled };
enum class Basis { fingerprint, comskip, both };
std::string_view kind_name(MarkKind kind);
std::string_view state_name(MarkState state);
std::string_view basis_name(Basis basis);

struct Mark {
    double start{}, end{};
    MarkKind kind{};
    MarkState state{};
    Basis basis{};
    bool operator==(const Mark&) const = default;
};

// One match: the entry's whole extent from..to, of which agreed_from..agreed_to
// has agreed so far (the rest is projected). A pass gives agreed parts only.
struct CombinerMatch {
    double from{}, to{};
    double agreed_from{}, agreed_to{};
    Status status{};  // ad, in-programme ad, intro or credits
};
CombinerMatch agreed_match(double from, double to, Status status);

struct CombineMode {
    bool final = true;
    double now{};  // live mode: the capture second reached
};

// Two ad stretches closer than this are one break unless programme lies between.
inline constexpr double join_gap = 90.0;
// Without a learned logo only shorter gaps are joined (Q9).
inline constexpr double join_gap_without_logo = 30.0;
// A logo interval at least this long is programme evidence.
inline constexpr double logo_closes = 5.0;
// A match edge moves to a seam this close.
inline constexpr double snap_reach = 1.0;
// Comskip blocks this close to a break touch it.
inline constexpr double touch_reach = 1.0;
// An ad match with the logo ending and resuming this close to it is ad audio
// over programme (marked in-programme ad, never skipped).
inline constexpr double bracket_reach = 8.0;
// Live: a mark with any unconfirmed part settles this long after its end.
inline constexpr double settle_after = 180.0;
// Mark edges that moved less than this did not change.
inline constexpr double unchanged_within = 0.05;

// Throws EvidenceMissing without evidence.
std::vector<Mark> combine(std::span<const CombinerMatch> matches, const Evidence* evidence, CombineMode mode);

// Live: a provisional mark settles 3 minutes after its end only when it has not
// changed for 3 minutes (roadmap Q9). Remembers when each mark last changed; a
// mark is the earlier one of its kind it overlaps by at least half the shorter
// (D3's rule), and it changed when either edge moved.
class MarkHistory {
public:
    // Marks from combine in live mode at capture second now: a mark combine
    // settled by age (3 minutes past its end) that changed less than 3 minutes
    // ago stays provisional.
    std::vector<Mark> settle(std::vector<Mark> marks, double now);

private:
    struct Seen {
        Mark mark;
        double since{};
    };
    std::vector<Seen> seen_;
};

// The within-show ad rule for a repeat inside one recording (Library, "ad,
// within one show"), from the recording's evidence: a learned logo seen during
// neither copy, seams within 1 s at both ends of at least one copy, and a
// standard length. A self-repeat match becomes an ad match only when it holds.
bool within_show_ad(const Evidence& evidence, double first_from, double first_to, double second_from, double second_to);

// The MarksFile for one capture, as one JSON object.
std::string marks_file_json(std::string_view capture_id, long long evidence_revision, bool final, std::span<const Mark> marks);
}
