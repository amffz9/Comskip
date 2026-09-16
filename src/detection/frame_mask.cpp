#include "../localization/diagnostic.h"
#include "frame_mask.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace comskip::detection {
namespace {
std::size_t columns(int value, std::size_t width) {
    if (value < 0 || static_cast<std::size_t>(value) > width)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::frame_mask_columns_exceed_image_width);
    return static_cast<std::size_t>(value);
}
}
std::size_t effective_mask_rows(int pixels, int percentage, std::size_t height) {
    if (pixels < 0 || static_cast<std::size_t>(pixels) > height)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::frame_mask_rows_exceed_image_height);
    if (percentage < 0 || percentage > 100)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::frame_mask_percentage_must_be_between_0_and_100);
    // Dividing first avoids overflow even for a size_t-sized image.
    return percentage == 0 ? static_cast<std::size_t>(pixels) :
        (height / 100) * static_cast<std::size_t>(percentage) +
        ((height % 100) * static_cast<std::size_t>(percentage)) / 100;
}
std::size_t frame_mask_storage_size(int width, int height, int stride) {
    if (width <= 0 || height <= 0 || stride < width)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_frame_mask_geometry);
    const auto rows = static_cast<std::size_t>(height);
    const auto pitch = static_cast<std::size_t>(stride);
    if (rows > std::numeric_limits<std::size_t>::max() / pitch)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::frame_mask_geometry_exceeds_addressable_storage);
    return rows * pitch;
}
void apply_frame_mask(std::span<unsigned char> pixels, int width, int height,
                      int stride, const FrameMaskOptions& options) {
    const auto required = frame_mask_storage_size(width, height, stride);
    if (pixels.size() < required)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::frame_mask_buffer_is_smaller_than_its_geometry);
    const auto w = static_cast<std::size_t>(width);
    const auto h = static_cast<std::size_t>(height);
    const auto bottom = effective_mask_rows(options.bottom_rows, options.bottom_percentage, h);
    const auto top = effective_mask_rows(options.top_rows, options.top_percentage, h);
    const auto side = columns(options.side_columns, w);
    const auto left = std::max(side, columns(options.left_columns, w));
    const auto right = std::max(side, columns(options.right_columns, w));
    if (bottom == 0 && top == 0 && left == 0 && right == 0) return;
    for (std::size_t y = 0; y < h; ++y) {
        auto row = pixels.subspan(y * static_cast<std::size_t>(stride), w);
        if (y < top || y >= h - bottom) {
            std::ranges::fill(row, 0);
        } else {
            std::ranges::fill(row.first(left), 0);
            std::ranges::fill(row.last(right), 0);
        }
    }
}
}
