#pragma once
#include <cstddef>
#include <cstdint>

namespace comskip::detection {
struct SceneSamplingGeometry {
    std::size_t storage_size;
    std::size_t initial_brightness_divisor;
    int step;
};
SceneSamplingGeometry validate_scene_sampling(int visible_width, int height, int stride, int border);
void validate_scene_brightness(int maximum, int test);
std::int64_t scaled_bright_pixel_limit(int maximum, int width, int height);
}
