#include "recording_context.h"
#include "image_geometry.h"

#include <algorithm>
#include <utility>

namespace {
template<class T>
void replace_buffer(std::vector<T>& buffer, std::size_t size, T value = {})
{
    if (buffer.size() != size) std::vector<T>(size, value).swap(buffer);
}
}

void RecordingState::ensure_pixel_buffers(bool use_logo)
{
    if (width == 0 && height == 0) return; // CSV metadata can precede its geometry.
    if (width > MAXWIDTH || height > MAXHEIGHT)
        throw std::invalid_argument("Detector image dimensions exceed supported limits");
    const auto size = comskip::detection::checked_image_size(width, height);
    const auto video_width = videowidth > 0 ? videowidth : width;
    if (video_width > width) throw std::invalid_argument("Detector video width exceeds its row stride");
    const bool changed = width != pixel_width || height != pixel_height || video_width != pixel_video_width;
    if (changed) {
        std::vector<char>(size).swap(haslogo);
        for (auto* buffer : {&horiz_count, &vert_count, &thoriz_edgemask, &tvert_edgemask,
                             &choriz_edgemask, &cvert_edgemask, &hor_edgecount, &ver_edgecount,
                             &max_br, &min_br}) std::vector<unsigned char>().swap(*buffer);
        logoFrameBuffer.clear();
        logoFrameNum.clear();
        logoFrameBufferSize = 0;
        lwidth = lheight = 0;
        newestLogoBuffer = -1;
        oldestLogoBuffer = 0;
        logoBuffersFull = false;
        edgemask_filled = 0;
        logoInfoAvailable = false;
        lastLogoTest = curLogoTest = false;
        currentGoodEdge = 0;
        logoTrendCounter = 0;
        tlogoMinX = tlogoMaxX = tlogoMinY = tlogoMaxY = 0;
        clogoMinX = clogoMaxX = clogoMinY = clogoMaxY = 0;
        pixel_width = width;
        pixel_height = height;
        pixel_video_width = video_width;
    }
    if (use_logo) {
        for (auto* buffer : {&horiz_count, &vert_count, &thoriz_edgemask, &tvert_edgemask,
                             &choriz_edgemask, &cvert_edgemask, &hor_edgecount, &ver_edgecount,
                             &max_br}) replace_buffer(*buffer, size);
        replace_buffer(min_br, size, static_cast<unsigned char>(255));
    }
}

void RecordingState::ensure_review_graph(int width, int height)
{
    const auto size = comskip::detection::checked_image_size(width, height, 3);
    replace_buffer(graph, size);
}
