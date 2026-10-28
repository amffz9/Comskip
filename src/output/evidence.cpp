#include "evidence.h"
#include "detection/frame_causes.h"
#include <nlohmann/json.hpp>
#include <array>
#include <charconv>
#include <cmath>
#include <ios>
#include <ostream>
#include <stdexcept>
#include <string>

namespace comskip::output {
namespace {
using comskip::detection::FrameCause;
using comskip::detection::cause_value;

unsigned seam_kinds(int cause) {
    unsigned kinds = 0;
    if (cause & (cause_value(FrameCause::black) | cause_value(FrameCause::non_uniform)))
        kinds |= static_cast<unsigned>(SeamKind::black);
    if (cause & cause_value(FrameCause::silence)) kinds |= static_cast<unsigned>(SeamKind::silence);
    if (cause & cause_value(FrameCause::scene_change)) kinds |= static_cast<unsigned>(SeamKind::scene);
    if (cause & cause_value(FrameCause::aspect_ratio)) kinds |= static_cast<unsigned>(SeamKind::aspect);
    return kinds;
}

// Times to three decimals, scores to two, written as the shortest text that
// reads back as the rounded value.
double rounded(double value, int decimals) {
    if (!std::isfinite(value)) throw std::invalid_argument("Evidence number is not finite");
    // The decimal text a fixed-point write gives (exact, ties to even), read back.
    std::array<char, 400> text{};
    const auto written = std::to_chars(text.data(), text.data() + text.size(), value, std::chars_format::fixed, decimals);
    if (written.ec != std::errc{}) throw std::invalid_argument("Evidence number cannot be formatted");
    double result = 0;
    std::from_chars(text.data(), written.ptr, result);
    return result;
}
}

std::vector<EvidenceSeam> evidence_seams(std::span<const black_frame_info> cut_frames,
                                         long end_frame, const FrameTime& time) {
    std::vector<EvidenceSeam> seams;
    std::size_t index = 0;
    while (index < cut_frames.size() && cut_frames[index].frame < end_frame) {
        const auto first = time(cut_frames[index].frame);
        auto last = first;
        unsigned kinds = seam_kinds(cut_frames[index].cause);
        for (++index; index < cut_frames.size() && cut_frames[index].frame < end_frame; ++index) {
            const auto next = time(cut_frames[index].frame);
            if (next - last >= 1.0) break;
            last = next;
            kinds |= seam_kinds(cut_frames[index].cause);
        }
        if (kinds) seams.push_back({(first + last) / 2, kinds});
    }
    return seams;
}

std::vector<EvidenceInterval> evidence_logo(std::span<const frame_info> frames, long first_frame,
                                            const FrameTime& time) {
    std::vector<EvidenceInterval> intervals;
    for (std::size_t index = 0; index < frames.size();) {
        if (!frames[index].logo_present) { ++index; continue; }
        const auto start = index;
        while (index < frames.size() && frames[index].logo_present) ++index;
        const auto end = index < frames.size() ? index : index - 1;
        intervals.push_back({time(first_frame + static_cast<long>(start)),
                             time(first_frame + static_cast<long>(end))});
    }
    return intervals;
}

void write_evidence(std::ostream& output, const Evidence& evidence) {
    if (!output) throw std::ios_base::failure("Evidence output stream is not writable");
    using Json = nlohmann::ordered_json;
    Json document = {{"revision", evidence.revision},
                     {"final", evidence.final},
                     {"logoKnown", evidence.logo_known},
                     {"seams", Json::array()},
                     {"logo", Json::array()},
                     {"blocks", Json::array()}};
    for (const auto& seam : evidence.seams) {
        auto kinds = Json::array();
        for (const auto& [kind, name] : {std::pair{SeamKind::black, "black"}, std::pair{SeamKind::silence, "silence"},
                                         std::pair{SeamKind::scene, "scene"}, std::pair{SeamKind::aspect, "aspect"}})
            if (seam.kinds & static_cast<unsigned>(kind)) kinds.push_back(name);
        document["seams"].push_back({{"at", rounded(seam.at, 3)}, {"kinds", std::move(kinds)}});
    }
    for (const auto& interval : evidence.logo)
        document["logo"].push_back({{"from", rounded(interval.from, 3)}, {"to", rounded(interval.to, 3)}});
    for (const auto& block : evidence.blocks)
        document["blocks"].push_back({{"from", rounded(block.from, 3)},
                                      {"to", rounded(block.to, 3)},
                                      {"score", block.score ? Json(rounded(*block.score, 2)) : Json(nullptr)},
                                      {"decision", block.decision == BlockDecision::commercial ? "commercial" : "programme"}});
    const auto text = document.dump() + "\n";
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output) throw std::ios_base::failure("Failed writing evidence output");
}
}
