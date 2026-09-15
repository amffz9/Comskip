#pragma once

#include <cstddef>
#include <cmath>
#include <limits>
#include <span>
#include <stdexcept>

namespace comskip::detection {
inline std::size_t checked_image_size(int width, int height, std::size_t channels = 1)
{
    if (width <= 0 || height <= 0 || channels == 0)
        throw std::invalid_argument("Image dimensions and channel count must be positive");
    const auto row = static_cast<std::size_t>(width);
    const auto rows = static_cast<std::size_t>(height);
    if (row > std::numeric_limits<std::size_t>::max() / rows || row * rows > std::numeric_limits<std::size_t>::max() / channels)
        throw std::length_error("Image dimensions exceed the addressable buffer size");
    return row * rows * channels;
}

inline void validate_logo_bounds(int width, int height, int minimum_x, int maximum_x,
                                int minimum_y, int maximum_y)
{
    if (width <= 0 || height <= 0 || minimum_x < 0 || minimum_y < 0 ||
        maximum_x < minimum_x || maximum_y < minimum_y || maximum_x >= width || maximum_y >= height)
        throw std::invalid_argument("Persisted logo bounds must lie inside the decoded image");
}

class LumaImageView {
    std::span<const unsigned char> pixels_;
    int stride_{};
    int width_{};
    int height_{};
public:
    LumaImageView(std::span<const unsigned char> pixels, int stride, int width, int height)
        : pixels_(pixels), stride_(stride), width_(width), height_(height)
    {
        if (width <= 0 || stride < width || pixels.size() < checked_image_size(stride, height))
            throw std::invalid_argument("Luma image must contain every decoded row");
    }
    // Rounded display canvases can extend beyond the source. Those pixels stay
    // black, rather than reading decoder padding or an adjacent image plane.
    unsigned char scaled_sample(int canvas_x, int canvas_y, double scale) const noexcept
    {
        const auto x = canvas_x * scale;
        const auto y = canvas_y * scale;
        if (!std::isfinite(scale) || scale <= 0 || !std::isfinite(x) || !std::isfinite(y) ||
            x < 0 || y < 0 || x >= width_ || y >= height_) return 0;
        return pixels_[static_cast<std::size_t>(y) * stride_ + static_cast<std::size_t>(x)];
    }
};
}
