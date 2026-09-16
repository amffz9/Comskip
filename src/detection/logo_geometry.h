#pragma once
#include <cstddef>
#include <vector>

namespace comskip::detection {
struct LogoScanOptions {
    int edge_radius;
    int edge_step;
    int border;
    bool at_side{};
    bool at_bottom{};
    bool subtitles{};
};
struct LogoScanGeometry {
    std::size_t storage_size;
    int expansion;
    int inset;
    int minimum_x, maximum_x, minimum_y, maximum_y;
    std::vector<int> columns, rows;
};
LogoScanGeometry validate_logo_scan(int width, int height, int stride, LogoScanOptions options);
struct LogoFilterHistory {
    int sample;
    int delay;
    int delayed_frame;
    int recent_begin;
    bool complete_windows;
};
LogoFilterHistory validate_logo_filter(int filter, int sample, int current_frame, std::size_t storage_size);
}
