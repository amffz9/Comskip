#pragma once
#include <cstddef>
#include <span>

namespace comskip::detection {
struct FrameMaskOptions {
    int bottom_rows{};
    int top_rows{};
    int bottom_percentage{};
    int top_percentage{};
    int side_columns{};
    int left_columns{};
    int right_columns{};
};

// Percentage zero selects the pixel setting; positive percentages override it.
std::size_t effective_mask_rows(int pixels, int percentage, std::size_t height);
std::size_t frame_mask_storage_size(int width, int height, int stride);
// Validates the entire request before changing pixels. Row padding is preserved.
void apply_frame_mask(std::span<unsigned char> pixels, int width, int height,
                      int stride, const FrameMaskOptions& options);
}
