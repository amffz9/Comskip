#pragma once
#include <optional>
#include <string_view>

namespace comskip::input {
// Owned detector observation decoded from legacy numeric CSV columns.
struct FrameRecord {
    int number{}, brightness{}, scene_change{}, logo{}, uniform{}, volume{};
    int min_y{}, max_y{};
    double aspect_ratio{}, good_edge{};
    int black{}, cutscene_match{}, min_x{}, max_x{}, bright_count{}, dim_count{};
    std::optional<double> timestamp;
    int segment{}, audio_channels = 2;
};
// rapidcsv owns delimiter/quoting syntax. This adapter applies Comskip's
// historical numeric scaling and optional-column defaults only.
FrameRecord parse_frame_record(std::string_view line);
// Header labels remain in file order; an optional final numeric rate preserves
// the old integer hundredths/millisecond encoding and modern decimal encoding.
std::optional<double> parse_frame_rate(std::string_view header);
}
