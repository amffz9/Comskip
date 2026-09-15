#pragma once

#include "output/edl.h"
#include <optional>
#include <string>

namespace comskip::output {
struct TimeInterval { Seconds start{}, end{}; };
struct ByteInterval { std::int64_t start{}, end{}; };
struct FrameInterval { FrameIndex start{}, end{}; };
struct SceneMarker { Seconds time{}; std::size_t sequence{}; };
struct ChapterSegment { TimeInterval time; bool commercial{}; };
struct StreamIds { int video{}, audio{}, subtitle{}; };
struct XmlMediaDescription {
    std::filesystem::path filename;
    std::filesystem::path demux_basename;
    std::optional<StreamIds> stream_ids;
};
struct CuttermaranOptions {
    // Legacy INI attribute list, parsed by pugixml rather than interpolated.
    std::string command_attributes;
};
struct MkvOptions {
    bool ordered_without_commercials{};
    std::string commercial_label{"Commercial"};
    std::string show_label{"Show"};
    std::string with_commercials_title{"With Commercials"};
    std::string without_commercials_title{"Without Commercials"};
};

// Complete UTF-8 XML documents. Inputs are already resolved into media times,
// byte positions or retained frame ranges; these functions own no application
// settings/state. Invalid ranges and failed streams throw before/after writing.
void write_videoredo3(std::ostream&, const XmlMediaDescription&,
                     std::span<const TimeInterval>, std::span<const SceneMarker>);
void write_edlx(std::ostream&, std::span<const ByteInterval>);
void write_btv(std::ostream&, std::span<const TimeInterval>);
void write_cuttermaran(std::ostream&, const XmlMediaDescription&,
                      std::span<const FrameInterval>, const CuttermaranOptions& = {});
void write_dvrmstb(std::ostream&, std::span<const TimeInterval>);
void write_mkv_chapters(std::ostream&, std::span<const ChapterSegment>, const MkvOptions& = {});
void write_mkv_tags(std::ostream&, const MkvOptions& = {});
}
