#include "output/xml_cutlists.h"

#include <pugixml.hpp>
#include <cmath>
#include <format>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace comskip::output {
namespace {
using Node = pugi::xml_node;
Node child(Node parent, const char* name) {
    auto result = parent.append_child(name);
    if (!result) throw std::bad_alloc{};
    return result;
}
void attribute(Node node, const char* name, const std::string& value) {
    if (!node.append_attribute(name).set_value(value.c_str())) throw std::bad_alloc{};
}
void text(Node parent, const char* name, const std::string& value) {
    if (!child(parent, name).text().set(value.c_str())) throw std::bad_alloc{};
}
std::string utf8(const std::filesystem::path& path) {
    auto value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}
void validate(Seconds value) {
    if (!std::isfinite(value.count()) || value.count() < 0 ||
        value.count() >= static_cast<double>(std::numeric_limits<std::int64_t>::max()) / 1e9)
        throw std::invalid_argument("Invalid XML media time");
}
template<class Range> void validate_ranges(std::span<const Range> ranges) {
    for (const auto& range : ranges) {
        if constexpr (std::is_same_v<Range, TimeInterval>) {
            validate(range.start); validate(range.end);
        } else if (range.start < 0 || range.end < 0) {
            throw std::invalid_argument("Negative XML range position");
        }
        if (range.end < range.start) throw std::invalid_argument("Reversed XML range");
    }
}
std::string ticks(Seconds value) {
    validate(value);
    return std::to_string(static_cast<std::int64_t>(std::round(value.count() * 10000000)));
}
std::string timestamp(Seconds value) {
    validate(value);
    const auto ns = static_cast<std::int64_t>(std::round(value.count() * 1e9));
    const auto seconds = ns / 1000000000;
    return std::format("{:02}:{:02}:{:02}.{:09}", seconds / 3600,
                       seconds / 60 % 60, seconds % 60, ns % 1000000000);
}
std::string timecode(Seconds value) {
    validate(value);
    const auto centiseconds = static_cast<std::int64_t>(value.count() * 100);
    return std::format("{}:{:02}:{:02}.{:02}", centiseconds / 360000,
                       centiseconds / 6000 % 60, centiseconds / 100 % 60, centiseconds % 100);
}
void declaration(pugi::xml_document& document, bool standalone = false) {
    auto node = document.append_child(pugi::node_declaration);
    if (!node) throw std::bad_alloc{};
    attribute(node, "version", "1.0");
    attribute(node, "encoding", "UTF-8");
    if (standalone) attribute(node, "standalone", "yes");
}
void save(std::ostream& output, pugi::xml_document& document) {
    document.save(output, "  ", pugi::format_default, pugi::encoding_utf8);
    if (!output) throw std::ios_base::failure("Writing XML cutlist failed");
}
void chapter(Node edition, const ChapterSegment& segment, const MkvOptions& options, bool ordered) {
    auto atom = child(edition, "ChapterAtom");
    text(child(atom, "ChapterDisplay"), "ChapterString",
         segment.commercial ? options.commercial_label : options.show_label);
    if (ordered) text(atom, "ChapterFlagEnabled", "1");
    text(atom, "ChapterTimeStart", timestamp(segment.time.start));
    if (ordered) text(atom, "ChapterTimeEnd", timestamp(segment.time.end));
}
}

void write_videoredo3(std::ostream& output, const XmlMediaDescription& media,
                     std::span<const TimeInterval> intervals, std::span<const SceneMarker> scenes) {
    validate_ranges(intervals);
    for (const auto& scene : scenes) validate(scene.time);
    pugi::xml_document document; declaration(document);
    auto root = child(document, "VideoReDoProject"); attribute(root, "Version", "3");
    text(root, "Filename", utf8(media.filename));
    auto list = child(root, "CutList");
    if (media.stream_ids) {
        auto pids = child(list, "InputPIDList");
        text(pids, "VideoStreamPID", std::to_string(media.stream_ids->video));
        text(pids, "AudioStreamPID", std::to_string(media.stream_ids->audio));
        text(pids, "SubtitlePID1", std::to_string(media.stream_ids->subtitle));
    }
    for (const auto& interval : intervals) {
        auto cut = child(list, "Cut");
        text(cut, "CutTimeStart", ticks(interval.start)); text(cut, "CutTimeEnd", ticks(interval.end));
    }
    auto scene_list = child(root, "SceneList");
    for (const auto& scene : scenes) {
        auto marker = child(scene_list, "SceneMarker");
        attribute(marker, "Sequence", std::to_string(scene.sequence));
        attribute(marker, "Timecode", timecode(scene.time));
        if (!marker.text().set(ticks(scene.time).c_str())) throw std::bad_alloc{};
    }
    save(output, document);
}
void write_edlx(std::ostream& output, std::span<const ByteInterval> intervals) {
    validate_ranges(intervals);
    pugi::xml_document document; declaration(document);
    auto root = child(document, "regionlist");
    attribute(root, "units", "bytes"); attribute(root, "mode", "exclude");
    for (const auto& interval : intervals) {
        auto region = child(root, "region");
        attribute(region, "start", std::to_string(interval.start));
        attribute(region, "end", std::to_string(interval.end));
    }
    save(output, document);
}
void write_btv(std::ostream& output, std::span<const TimeInterval> intervals) {
    validate_ranges(intervals);
    pugi::xml_document document; declaration(document);
    auto root = child(document, "cutlist");
    for (const auto& interval : intervals) {
        auto region = child(root, "Region");
        for (auto [name, value] : {std::pair{"start", interval.start}, std::pair{"end", interval.end}}) {
            auto node = child(region, name); attribute(node, "comment", timecode(value));
            if (!node.text().set(ticks(value).c_str())) throw std::bad_alloc{};
        }
    }
    save(output, document);
}
void write_cuttermaran(std::ostream& output, const XmlMediaDescription& media,
                      std::span<const FrameInterval> retained, const CuttermaranOptions& options) {
    validate_ranges(retained);
    pugi::xml_document document; declaration(document, true);
    auto root = child(document, "StateData");
    attribute(root, "xmlns", "http://cuttermaran.kickme.to/StateData.xsd");
    const auto basename = utf8(media.demux_basename);
    auto video = child(root, "usedVideoFiles"); attribute(video, "FileID", "0");
    attribute(video, "FileName", basename + ".M2V");
    auto audio = child(root, "usedAudioFiles"); attribute(audio, "FileID", "1");
    attribute(audio, "FileName", basename + ".mp2"); attribute(audio, "StartDelay", "0");
    for (const auto& interval : retained) {
        auto cut = child(root, "CutElements"); attribute(cut, "refVideoFile", "0");
        attribute(cut, "StartPosition", std::to_string(interval.start));
        attribute(cut, "EndPosition", std::to_string(interval.end));
        attribute(child(cut, "CurrentFiles"), "refVideoFiles", "0");
        attribute(child(cut, "cutAudioFiles"), "refAudioFile", "1");
    }
    auto args = child(root, "CmdArgs"); attribute(args, "OutFile", basename + "_clean.m2v");
    if (options.command_attributes.empty()) {
        for (const auto* name : {"cut", "unattended", "snapToCutPoints", "closeApp"}) attribute(args, name, "true");
    } else {
        pugi::xml_document fragment;
        const auto source = "<CmdArgs " + options.command_attributes + " />";
        if (!fragment.load_string(source.c_str()) || fragment.first_child().next_sibling() ||
            fragment.first_child().first_child()) throw std::invalid_argument("Invalid Cuttermaran attributes");
        for (const auto& attr : fragment.first_child().attributes()) {
            if (args.attribute(attr.name())) throw std::invalid_argument("Duplicate Cuttermaran attribute");
            attribute(args, attr.name(), attr.value());
        }
    }
    save(output, document);
}
void write_dvrmstb(std::ostream& output, std::span<const TimeInterval> intervals) {
    validate_ranges(intervals);
    pugi::xml_document document; declaration(document);
    auto root = child(document, "root");
    for (const auto& interval : intervals) {
        auto commercial = child(root, "commercial");
        attribute(commercial, "start", std::format("{:.6f}", interval.start.count()));
        attribute(commercial, "end", std::format("{:.6f}", interval.end.count()));
    }
    save(output, document);
}
void write_mkv_chapters(std::ostream& output, std::span<const ChapterSegment> segments, const MkvOptions& options) {
    std::vector<ChapterSegment> merged;
    for (const auto& segment : segments) {
        validate_ranges(std::span{&segment.time, 1});
        if (!merged.empty() && segment.time.start < merged.back().time.end)
            throw std::invalid_argument("Overlapping MKV chapters");
        if (!merged.empty() && merged.back().commercial == segment.commercial &&
            merged.back().time.end == segment.time.start) merged.back().time.end = segment.time.end;
        else merged.push_back(segment);
    }
    pugi::xml_document document; declaration(document);
    auto root = child(document, "Chapters");
    auto edition = child(root, "EditionEntry"); text(edition, "EditionUID", "1");
    for (const auto& segment : merged) chapter(edition, segment, options, false);
    if (options.ordered_without_commercials) {
        auto ordered = child(root, "EditionEntry"); text(ordered, "EditionUID", "2");
        text(ordered, "EditionFlagOrdered", "1");
        for (const auto& segment : merged) if (!segment.commercial) chapter(ordered, segment, options, true);
    }
    save(output, document);
}
void write_mkv_tags(std::ostream& output, const MkvOptions& options) {
    pugi::xml_document document; declaration(document);
    auto root = child(document, "Tags");
    for (int id : {1, 2}) {
        auto tag = child(root, "Tag"); auto targets = child(tag, "Targets");
        text(targets, "TargetTypeValue", "50"); text(targets, "EditionUID", std::to_string(id));
        auto simple = child(tag, "Simple"); text(simple, "TagLanguage", "eng");
        text(simple, "Name", "TITLE"); text(simple, "DefaultLanguage", "1");
        text(simple, "String", id == 1 ? options.with_commercials_title : options.without_commercials_title);
    }
    save(output, document);
}
}
