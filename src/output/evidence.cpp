#include "evidence.h"
#include "detection/frame_causes.h"
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

void append_number(std::string& text, double value, int decimals) {
    if (!std::isfinite(value)) throw std::invalid_argument("Evidence number is not finite");
    std::array<char, 512> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
                                      std::chars_format::fixed, decimals);
    if (result.ec != std::errc{}) throw std::invalid_argument("Evidence number cannot be formatted");
    text.append(buffer.data(), result.ptr);
}

void append_interval(std::string& text, double from, double to) {
    text += "{ \"from\": ";
    append_number(text, from, 3);
    text += ", \"to\": ";
    append_number(text, to, 3);
}

template <class T, class Write>
void append_array(std::string& text, const char* name, const std::vector<T>& items, Write write) {
    text += ",\n  \"";
    text += name;
    text += "\": [";
    for (std::size_t index = 0; index < items.size(); ++index) {
        text += index ? ",\n    " : "\n    ";
        write(items[index]);
    }
    text += items.empty() ? "]" : " ]";
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
    std::string text = "{ \"revision\": " + std::to_string(evidence.revision) +
        ", \"final\": " + (evidence.final ? "true" : "false") +
        ", \"logoKnown\": " + (evidence.logo_known ? "true" : "false");
    append_array(text, "seams", evidence.seams, [&](const EvidenceSeam& seam) {
        text += "{ \"at\": ";
        append_number(text, seam.at, 3);
        text += ", \"kinds\": [";
        const char* separator = "";
        for (const auto& [kind, name] : {std::pair{SeamKind::black, "black"}, std::pair{SeamKind::silence, "silence"},
                                         std::pair{SeamKind::scene, "scene"}, std::pair{SeamKind::aspect, "aspect"}}) {
            if (!(seam.kinds & static_cast<unsigned>(kind))) continue;
            text += separator;
            text += '"';
            text += name;
            text += '"';
            separator = ", ";
        }
        text += "] }";
    });
    append_array(text, "logo", evidence.logo, [&](const EvidenceInterval& interval) {
        append_interval(text, interval.from, interval.to);
        text += " }";
    });
    append_array(text, "blocks", evidence.blocks, [&](const EvidenceBlock& block) {
        append_interval(text, block.from, block.to);
        text += ", \"score\": ";
        if (block.score) append_number(text, *block.score, 2);
        else text += "null";
        text += ", \"decision\": ";
        text += block.decision == BlockDecision::commercial ? "\"commercial\"" : "\"programme\"";
        text += " }";
    });
    text += " }\n";
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output) throw std::ios_base::failure("Failed writing evidence output");
}
}
