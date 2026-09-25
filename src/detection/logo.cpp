#include "../localization/diagnostic.h"
#include "app/debug.h"
#include "app/recording_context.h"
#include "block_building.h"
#include "config/legacy_settings.h"
#include "detection_methods.h"
#include "detector_runtime.h"
#include "frame_timestamps.h"
#include "image_geometry.h"
#include "logo_sampling.h"
#include "logo_detection.h"
#include "logo_geometry.h"
#include "logo_shrink.h"
#include "saved_logo.h"
#include "storage.h"
#include "platform/file_resources.h"
#include "platform/platform.h"
#include "output/checked_file.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstdio>
#include <format>
#include <string_view>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
constexpr int horizontal_edge_direction = 0;
constexpr int vertical_edge_direction = 1;
constexpr int diagonal_1_edge_direction = 2;
constexpr int diagonal_2_edge_direction = 3;
constexpr int maximum_saved_logo_width = 2000;
constexpr int maximum_saved_logo_height = 1200;
constexpr int aspect_ratio_exclusion_distance = 20;
constexpr double maximum_edge_search_fraction = 0.02;

double frame_duration(RecordingContext& context, const long end_frame, const long start_frame) {
    return get_frame_pts(context, static_cast<int>(end_frame)) -
           get_frame_pts(context, static_cast<int>(start_frame));
}

comskip::detection::LogoScanGeometry logo_scan(const RecordingContext& context) {
    return comskip::detection::validate_logo_scan(context.state.videowidth,
        context.state.height, context.state.width,
        {context.settings.edge_radius, context.settings.edge_step, context.settings.border,
         context.settings.logo_at_side != 0, context.settings.logo_at_bottom != 0,
         context.settings.subtitles != 0});
}
void require_logo_buffer(std::size_t available, const comskip::detection::LogoScanGeometry& scan) {
    if (available < scan.storage_size)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::logo_scan_requires_complete_geometry_sized_pixel_buffers);
}
template <typename... Args>
void LogoDebug(RecordingContext& context, int level, std::string_view key, Args&&... args) {
    Debug(context, level, context.translator.format(key, std::forward<Args>(args)...));
}
std::string logo_caption_type(RecordingContext& context, int type) {
    if (!context.state.processCC) return {};
    switch (type) {
    case comskip::detection::caption_type_value(comskip::detection::CaptionType::none): return context.translator.text("caption_type_none");
    case comskip::detection::caption_type_value(comskip::detection::CaptionType::rollup): return context.translator.text("caption_type_rollup");
    case comskip::detection::caption_type_value(comskip::detection::CaptionType::painton): return context.translator.text("caption_type_painton");
    case comskip::detection::caption_type_value(comskip::detection::CaptionType::popon): return context.translator.text("caption_type_popon");
    case comskip::detection::caption_type_value(comskip::detection::CaptionType::commercial): return context.translator.text("caption_type_commercial");
    default: return std::format("{}", type);
    }
}
}

void PrintLogoFrameGroups(RecordingContext& context)
{
    int		i,l;
    double  cl;
    int		f,t;
    int		count = 0;

    LogoDebug(context, 2, "logo_detected_heading");
    count = 0;
    for (i = 0; i < context.state.logo_block_count; i++)
    {
        f = FindBlock(context, context.state.logo_block[i].start);
        t = FindBlock(context, context.state.logo_block[i].end-2);
        if (t < 0)
        {
            LogoDebug(context, 2, "logo_block_lookup_failed");
            break;
        }
        if (f < 0)
        {
            LogoDebug(context, 2, "logo_block_lookup_failed");
            break;
        }
        LogoDebug(context, 2, "logo_block_row",
            std::format("{:6}", context.state.logo_block[i].start),
            std::format("{:6}", context.state.logo_block[i].end),
            dblSecondsToStrMinutes(frame_duration(context, context.state.logo_block[i].end, context.state.logo_block[i].start)),
            std::format("{:.1f}", frame_duration(context, context.state.logo_block[i].start, context.state.cblock[f].f_start)),
            std::format("{:.1f}", frame_duration(context, context.state.cblock[t].f_end, context.state.logo_block[i].end)));

        count += context.state.logo_block[i].end - context.state.logo_block[i].start + 1;

    }
    for (i = 0; i < context.state.logo_block_count-1; i++)
    {
        f = context.state.logo_block[i].end;
        t = context.state.logo_block[i+1].start;
        if (context.state.max_logo_gap < frame_duration(context, t,f))
            context.state.max_logo_gap = frame_duration(context, t,f);
        f = FindBlock(context, context.state.logo_block[i].end);
        t = FindBlock(context, context.state.logo_block[i+1].start);
        for (l = f+1; l < t; l++)
        {
            if (context.state.max_nonlogo_block_length < context.state.cblock[l].length)
                context.state.max_nonlogo_block_length = context.state.cblock[l].length;
        }
    }
    for (i = 0; i < context.state.logo_block_count-1; i++)
    {
        f = FindBlock(context, context.state.logo_block[i].start);
        t = FindBlock(context, context.state.logo_block[i].end);
        if (frame_duration(context, context.state.logo_block[i].end, context.state.logo_block[i].start) > context.state.max_nonlogo_block_length )
        {
            cl = frame_duration(context, context.state.cblock[f].f_end, context.state.logo_block[i].start);
            if (cl < context.state.cblock[f].length / 10 )
            {
                if (cl > context.state.logo_overshoot )
                    context.state.logo_overshoot = cl;
            }
            cl = frame_duration(context, context.state.logo_block[i].end, context.state.cblock[t].f_start);
            if (cl < context.state.cblock[t].length / 10 )
            {
                if (cl > context.state.logo_overshoot)
                    context.state.logo_overshoot = cl;
            }
        }
    }
    if (context.state.logo_overshoot > 0)
        context.state.logo_overshoot = context.state.logo_overshoot + 1 + context.settings.shrink_logo;
    else
        context.state.logo_overshoot = context.settings.shrink_logo;
}

void PrintCCBlocks(RecordingContext& context)
{
    int i, j;
    const auto duration = [&](int end, int start) {
        return context.settings.fps > 0 ? frame_duration(context, end, start) : 0.0;
    };
    LogoDebug(context, 2, "logo_combining_cc_blocks");
    for (i = context.state.cc_block_count - 1; i > 0; i--)
    {
        if (duration(context.state.cc_block[i].end_frame,
                context.state.cc_block[i].start_frame) < 1.0)
        {
            LogoDebug(context, 4, "logo_removing_cc_block", std::format("{}", i),
                std::format("{:.2f}", duration(context.state.cc_block[i].end_frame,
                    context.state.cc_block[i].start_frame)));
            for (j = i; j < context.state.cc_block_count - 1; j++)
            {
                context.state.cc_block[j].start_frame = context.state.cc_block[j + 1].start_frame;
                context.state.cc_block[j].end_frame = context.state.cc_block[j + 1].end_frame;
                context.state.cc_block[j].type = context.state.cc_block[j + 1].type;
            }

            context.state.cc_block_count--;
        }
    }

    LogoDebug(context, 2, "logo_cc_detected_heading", std::format("{}", context.state.cc_block_count));
    if (context.state.cc_block.empty()) {
        context.state.most_cc_type = comskip::detection::caption_type_value(comskip::detection::CaptionType::none);
        return;
    }
    LogoDebug(context, 2, "logo_cc_block_row", " 0",
        std::format("{:6}", context.state.cc_block[0].start_frame),
        std::format("{:6}", context.state.cc_block[0].end_frame),
        logo_caption_type(context, context.state.cc_block[0].type));
    LogoDebug(context, 2, "logo_cc_block_length",
        dblSecondsToStrMinutes(duration(context.state.cc_block[0].end_frame,
            context.state.cc_block[0].start_frame)));
    context.state.cc_count[context.state.cc_block[0].type] += context.state.cc_block[0].end_frame - context.state.cc_block[0].start_frame + 1;

    for (i = 1; i < context.state.cc_block_count; i++)
    {
        LogoDebug(context, 2, "logo_cc_block_row", std::format("{:2}", i),
            std::format("{:6}", context.state.cc_block[i].start_frame),
            std::format("{:6}", context.state.cc_block[i].end_frame),
            logo_caption_type(context, context.state.cc_block[i].type));
        LogoDebug(context, 2, "logo_cc_block_length",
            dblSecondsToStrMinutes(duration(context.state.cc_block[i].end_frame,
                context.state.cc_block[i].start_frame)));
        context.state.cc_count[context.state.cc_block[i].type] += context.state.cc_block[i].end_frame - context.state.cc_block[i].start_frame + 1;
    }

    LogoDebug(context, 2, "logo_caption_sums_heading");
    const auto caption_sum = [&](const char* label, int type) {
        const auto percentage = context.state.framesprocessed > 0
            ? static_cast<double>(context.state.cc_count[type]) / context.state.framesprocessed * 100.0 : 0.0;
        const auto seconds = context.settings.fps > 0
            ? context.state.cc_count[type] / context.settings.fps : 0.0;
        LogoDebug(context, 2, "logo_caption_sum", label,
            std::format("{:6}", context.state.cc_count[type]), std::format("{:5.2f}", percentage),
            dblSecondsToStrMinutes(seconds));
    };
    caption_sum(context.translator.text("logo_caption_popon"), comskip::detection::caption_type_value(comskip::detection::CaptionType::popon));
    caption_sum(context.translator.text("logo_caption_rollup"), comskip::detection::caption_type_value(comskip::detection::CaptionType::rollup));
    caption_sum(context.translator.text("logo_caption_painton"), comskip::detection::caption_type_value(comskip::detection::CaptionType::painton));
    caption_sum(context.translator.text("logo_caption_none"), comskip::detection::caption_type_value(comskip::detection::CaptionType::none));
    for (i = 0; i <= 4; i++)
    {
        if (context.state.cc_count[i] > context.state.cc_count[context.state.most_cc_type])
        {
            context.state.most_cc_type = i;
        }
    }

    LogoDebug(context, 2, "logo_caption_most_common",
        logo_caption_type(context, context.state.most_cc_type));
}


struct EdgeTests {
    int width;
    int radius;
    int threshold;

    [[nodiscard]] bool horizontal1(const unsigned char* frame, int x, int y) const noexcept {
        return std::abs(static_cast<int>(frame[y * width + x - radius]) -
            static_cast<int>(frame[y * width + x + radius])) >= threshold;
    }
    [[nodiscard]] bool vertical1(const unsigned char* frame, int x, int y) const noexcept {
        return std::abs(static_cast<int>(frame[(y - radius) * width + x]) -
            static_cast<int>(frame[(y + radius) * width + x])) >= threshold;
    }
    [[nodiscard]] bool horizontal0(const unsigned char* frame, int x, int y) const noexcept {
        const auto center = static_cast<int>(frame[y * width + x]);
        return std::abs(static_cast<int>(frame[y * width + x - radius]) - center) >= threshold ||
            std::abs(static_cast<int>(frame[y * width + x + radius]) - center) >= threshold;
    }
    [[nodiscard]] bool vertical0(const unsigned char* frame, int x, int y) const noexcept {
        const auto center = static_cast<int>(frame[y * width + x]);
        return std::abs(static_cast<int>(frame[(y - radius) * width + x]) - center) >= threshold ||
            std::abs(static_cast<int>(frame[(y + radius) * width + x]) - center) >= threshold;
    }
    [[nodiscard]] bool horizontal2(const unsigned char* frame, int x, int y) const noexcept {
        const auto left = static_cast<int>(frame[y * width + x - radius - 1]) +
            frame[y * width + x - radius] + frame[y * width + x - radius + 1];
        const auto right = static_cast<int>(frame[y * width + x + radius - 1]) +
            frame[y * width + x + radius] + frame[y * width + x + radius + 1];
        return std::abs(left - right) / 3 >= threshold;
    }
    [[nodiscard]] bool vertical2(const unsigned char* frame, int x, int y) const noexcept {
        const auto top = static_cast<int>(frame[(y - radius - 1) * width + x]) +
            frame[(y - radius) * width + x] + frame[(y - radius + 1) * width + x];
        const auto bottom = static_cast<int>(frame[(y + radius - 1) * width + x]) +
            frame[(y + radius) * width + x] + frame[(y + radius + 1) * width + x];
        return std::abs(top - bottom) / 3 >= threshold;
    }
    [[nodiscard]] bool horizontal3(const unsigned char* frame, int x, int y) const noexcept {
        const auto difference =
            static_cast<int>(frame[(y - radius) * width + x - radius]) - frame[(y - radius) * width + x + radius] +
            static_cast<int>(frame[y * width + x - radius]) - frame[y * width + x + radius] +
            static_cast<int>(frame[(y + radius) * width + x - radius]) - frame[(y + radius) * width + x + radius];
        return std::abs(difference) >= threshold;
    }
    [[nodiscard]] bool vertical3(const unsigned char* frame, int x, int y) const noexcept {
        const auto difference =
            static_cast<int>(frame[(y - radius) * width + x - radius]) - frame[(y + radius) * width + x - radius] +
            static_cast<int>(frame[(y - radius) * width + x]) - frame[(y + radius) * width + x] +
            static_cast<int>(frame[(y - radius) * width + x + radius]) - frame[(y + radius) * width + x + radius];
        return std::abs(difference) >= threshold;
    }
};


void EdgeDetect(RecordingContext& context, std::span<const unsigned char> frame, int maskNumber)
{
    const auto scan = logo_scan(context);
    if (frame.size() < static_cast<std::size_t>(context.state.width) * context.state.height)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::logo_edge_detection_requires_image_pixels);
    const auto* frame_ptr = frame.data();
    require_logo_buffer(context.state.hor_edgecount.size(), scan);
    require_logo_buffer(context.state.ver_edgecount.size(), scan);
    const EdgeTests edge_tests{context.state.width, context.settings.edge_radius,
        context.settings.edge_level_threshold};

    int				x;
    int				y;
    //	unsigned char	temp[MAXWIDTH * MAXHEIGHT];
//	memset(for (i = 0; i <= (width * height); i++) temp[i] = 0;
    context.state.hedge_count = 0;
    context.state.vedge_count = 0;
    if (context.settings.aggressive_logo_rejection==1)
    {
        for (const auto x : scan.columns)
        {
            for (const auto y : scan.rows) {
                if (edge_tests.horizontal1(frame_ptr,x,y))
                {
                    if (context.state.hor_edgecount[y * context.state.width + x] < context.settings.num_logo_buffers)
                        context.state.hor_edgecount[y * context.state.width + x]++;
                    else
                        context.state.edge_count++;
                }
                else
                    context.state.hor_edgecount[y * context.state.width + x] = 0;
                if (edge_tests.vertical1(frame_ptr,x,y))
                {
                    if (context.state.ver_edgecount[y * context.state.width + x] < context.settings.num_logo_buffers)
                        context.state.ver_edgecount[y * context.state.width + x]++;
                    else
                        context.state.edge_count++;
                }
                else
                    context.state.ver_edgecount[y * context.state.width + x] = 0;
            }
        }
    }
    else if (context.settings.aggressive_logo_rejection==2)
    {
        for (const auto x : scan.columns)
        {
            for (const auto y : scan.rows) {
                if (edge_tests.horizontal2(frame_ptr,x,y))
                {
                    if (context.state.hor_edgecount[y * context.state.width + x] < context.settings.num_logo_buffers)
                        context.state.hor_edgecount[y * context.state.width + x]++;
                    else
                        context.state.edge_count++;
                }
                else
                    context.state.hor_edgecount[y * context.state.width + x] = 0;
                if (edge_tests.vertical2(frame_ptr,x,y))
                {
                    if (context.state.ver_edgecount[y * context.state.width + x] < context.settings.num_logo_buffers)
                        context.state.ver_edgecount[y * context.state.width + x]++;
                    else
                        context.state.edge_count++;
                }
                else
                    context.state.ver_edgecount[y * context.state.width + x] = 0;
            }
        }
//	printf("%6d %6d\n", hedge_count, vedge_count);
    }
    else if (context.settings.aggressive_logo_rejection==3)
    {
        for (const auto x : scan.columns)
        {
            for (const auto y : scan.rows) {
                if (edge_tests.horizontal3(frame_ptr,x,y))
                {
                    if (context.state.hor_edgecount[y * context.state.width + x] < context.settings.num_logo_buffers)
                        context.state.hor_edgecount[y * context.state.width + x]++;
                    else
                        context.state.edge_count++;
                }
                else
                    context.state.hor_edgecount[y * context.state.width + x] = 0;
                if (edge_tests.vertical3(frame_ptr,x,y))
                {
                    if (context.state.ver_edgecount[y * context.state.width + x] < context.settings.num_logo_buffers)
                        context.state.ver_edgecount[y * context.state.width + x]++;
                    else
                        context.state.edge_count++;
                }
                else
                    context.state.ver_edgecount[y * context.state.width + x] = 0;
            }
        }
    }
    else if (context.settings.aggressive_logo_rejection==4)
    {
        for (const auto x : scan.columns)
        {
            for (const auto y : scan.rows) {
                if ((/*frame_ptr[y * width + x - edge_radius] > 50 && */ frame_ptr[y * context.state.width + x - context.settings.edge_radius] < 200) || ( /*frame_ptr[y * width + x + edge_radius] > 50 && */ frame_ptr[y * context.state.width + x + context.settings.edge_radius] < 200) )
                {
                    if (edge_tests.horizontal0(frame_ptr,x,y))
                    {
                        if (context.state.hor_edgecount[y * context.state.width + x] < context.settings.num_logo_buffers)
                            context.state.hor_edgecount[y * context.state.width + x]++;
                        else
                            context.state.edge_count++;
                    }
                    else if (frame_ptr[y * context.state.width + x] < 200)
                        context.state.hor_edgecount[y * context.state.width + x] = 0;
                }
                if ((/*frame_ptr[(y- edge_radius) * width + x ] > 50 && */ frame_ptr[(y- context.settings.edge_radius) * context.state.width + x ] < 200) || ( /*frame_ptr[(y+ edge_radius) * width + x ] > 50 && */ frame_ptr[(y+ context.settings.edge_radius) * context.state.width + x ] < 200) )
                {
                    if (edge_tests.vertical0(frame_ptr,x,y))
                    {
                        if (context.state.ver_edgecount[y * context.state.width + x] < context.settings.num_logo_buffers)
                            context.state.ver_edgecount[y * context.state.width + x]++;
                        else
                            context.state.edge_count++;
                    }
                    else if (frame_ptr[y * context.state.width + x] < 200)
                        context.state.ver_edgecount[y * context.state.width + x] = 0;
                }
            }
        }
    }
    else
    {
        for (const auto x : scan.columns)
        {
            for (const auto y : scan.rows) {
                if ((/*frame_ptr[y * width + x - edge_radius] > 50 && */ frame_ptr[y * context.state.width + x - context.settings.edge_radius] < 200) || ( /*frame_ptr[y * width + x + edge_radius] > 50 && */ frame_ptr[y * context.state.width + x + context.settings.edge_radius] < 200) )
                {
                    if (edge_tests.horizontal0(frame_ptr,x,y))
                    {
                        if (context.state.hor_edgecount[y * context.state.width + x] < context.settings.num_logo_buffers)
                            context.state.hor_edgecount[y * context.state.width + x]++;
                        else
                            context.state.edge_count++;
                    }
                    else
                        context.state.hor_edgecount[y * context.state.width + x] = 0;
                }
                if ((/*frame_ptr[(y- edge_radius) * width + x ] > 50 && */ frame_ptr[(y- context.settings.edge_radius) * context.state.width + x ] < 200) || ( /*frame_ptr[(y+ edge_radius) * width + x ] > 50 && */ frame_ptr[(y+ context.settings.edge_radius) * context.state.width + x ] < 200) )
                {
                    if (edge_tests.vertical0(frame_ptr,x,y))
                    {
                        if (context.state.ver_edgecount[y * context.state.width + x] < context.settings.num_logo_buffers)
                            context.state.ver_edgecount[y * context.state.width + x]++;
                        else
                            context.state.edge_count++;
                    }
                    else
                        context.state.ver_edgecount[y * context.state.width + x] = 0;
                }
            }
        }
    }
}


double CheckStationLogoEdge(RecordingContext& context, std::span<const unsigned char> frame)
{
    const auto scan = logo_scan(context);
    if (frame.size() < static_cast<std::size_t>(context.state.width) * context.state.height)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::logo_comparison_requires_image_pixels);
    const auto* testFrame = frame.data();
    require_logo_buffer(context.state.choriz_edgemask.size(), scan);
    require_logo_buffer(context.state.cvert_edgemask.size(), scan);
    const EdgeTests edge_tests{context.state.width, context.settings.edge_radius,
        context.settings.edge_level_threshold};

    int		index;
    int		x;
    int		y;
    int		testEdges = 0;

    int goodEdges = 0;

    context.state.currentGoodEdge = 0.0;
    if (context.state.videowidth < context.state.clogoMinX || context.state.height < context.state.clogoMinY)
    {
        // No logo possible as frame size if different from where logo was found
    }
    else if (context.settings.aggressive_logo_rejection == 1)
    {
        for (y = std::max(context.state.clogoMinY, scan.minimum_y); y <= std::min(context.state.clogoMaxY, scan.maximum_y); y += context.settings.edge_step)
        {
            for (x = std::max(context.state.clogoMinX, scan.minimum_x); x <= std::min(context.state.clogoMaxX, scan.maximum_x); x += context.settings.edge_step)
            {
                index = y * context.state.width + x;
                if (context.state.choriz_edgemask[index])
                {
                    if (edge_tests.horizontal1(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (context.state.cvert_edgemask[index])
                {
                    if (edge_tests.vertical1(testFrame,x,y))
                    {
                        goodEdges++;
                    }

                    testEdges++;
                }
            }
        }
    }
    else if (context.settings.aggressive_logo_rejection == 2)
    {
        for (y = std::max(context.state.clogoMinY, scan.minimum_y); y <= std::min(context.state.clogoMaxY, scan.maximum_y); y += context.settings.edge_step)
        {
            for (x = std::max(context.state.clogoMinX, scan.minimum_x); x <= std::min(context.state.clogoMaxX, scan.maximum_x); x += context.settings.edge_step)
            {
                index = y * context.state.width + x;
                if (context.state.choriz_edgemask[index])
                {
                    if (edge_tests.horizontal2(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (context.state.cvert_edgemask[index])
                {
                    if (edge_tests.vertical2(testFrame,x,y))
                    {
                        goodEdges++;
                    }

                    testEdges++;
                }
            }
        }
    }
    else if (context.settings.aggressive_logo_rejection == 3)
    {
        for (y = std::max(context.state.clogoMinY, scan.minimum_y); y <= std::min(context.state.clogoMaxY, scan.maximum_y); y += context.settings.edge_step)
        {
            for (x = std::max(context.state.clogoMinX, scan.minimum_x); x <= std::min(context.state.clogoMaxX, scan.maximum_x); x += context.settings.edge_step)
            {
                index = y * context.state.width + x;
                if (context.state.choriz_edgemask[index])
                {
                    if (edge_tests.horizontal3(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (context.state.cvert_edgemask[index])
                {
                    if (edge_tests.vertical3(testFrame,x,y))
                    {
                        goodEdges++;
                    }

                    testEdges++;
                }
            }
        }
    }
    else if (context.settings.aggressive_logo_rejection == 4)
    {
        for (y = std::max(context.state.clogoMinY, scan.minimum_y); y <= std::min(context.state.clogoMaxY, scan.maximum_y); y += context.settings.edge_step)
        {
            for (x = std::max(context.state.clogoMinX, scan.minimum_x); x <= std::min(context.state.clogoMaxX, scan.maximum_x); x += context.settings.edge_step)
            {
                index = y * context.state.width + x;
                if (context.state.choriz_edgemask[index] && testFrame[index] < 200)
                {
                    if (edge_tests.horizontal0(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (context.state.cvert_edgemask[index] && testFrame[index] < 200)
                {
                    if (edge_tests.vertical0(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
            }
        }
    }
    else
    {
        for (y = std::max(context.state.clogoMinY, scan.minimum_y); y <= std::min(context.state.clogoMaxY, scan.maximum_y); y += context.settings.edge_step)
        {
            for (x = std::max(context.state.clogoMinX, scan.minimum_x); x <= std::min(context.state.clogoMaxX, scan.maximum_x); x += context.settings.edge_step)
            {
                index = y * context.state.width + x;
                if (context.state.choriz_edgemask[index])
                {
                    if (edge_tests.horizontal0(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (context.state.cvert_edgemask[index])
                {
                    if (edge_tests.vertical0(testFrame,x,y))
                    {
                        goodEdges++;
                    }

                    testEdges++;
                }
            }
        }
    }
    if (testEdges == 0)
        return(0.5);
    return static_cast<double>(goodEdges) / static_cast<double>(testEdges);
}

double DoubleCheckStationLogoEdge(RecordingContext& context, unsigned char* testFrame)
{
    const auto scan = logo_scan(context);
    if (!testFrame) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::logo_comparison_requires_image_pixels);
    require_logo_buffer(context.state.thoriz_edgemask.size(), scan);
    require_logo_buffer(context.state.tvert_edgemask.size(), scan);
    const EdgeTests edge_tests{context.state.width, context.settings.edge_radius,
        context.settings.edge_level_threshold};

    int		index;
    int		x;
    int		y;
    int		testEdges = 0;

    int goodEdges = 0;

    context.state.currentGoodEdge = 0.0;
    if (context.settings.aggressive_logo_rejection == 1)
    {
        for (y = std::max(context.state.tlogoMinY, scan.minimum_y); y <= std::min(context.state.tlogoMaxY, scan.maximum_y); y += context.settings.edge_step)
        {
            for (x = std::max(context.state.tlogoMinX, scan.minimum_x); x <= std::min(context.state.tlogoMaxX, scan.maximum_x); x += context.settings.edge_step)
            {
                index = y * context.state.width + x;
                if (context.state.thoriz_edgemask[index])
                {
                    if (edge_tests.horizontal1(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (context.state.tvert_edgemask[index])
                {
                    if (edge_tests.vertical1(testFrame,x,y))
                    {
                        goodEdges++;
                    }

                    testEdges++;
                }
            }
        }
    }
    else if (context.settings.aggressive_logo_rejection == 2)
    {
        for (y = std::max(context.state.tlogoMinY, scan.minimum_y); y <= std::min(context.state.tlogoMaxY, scan.maximum_y); y += context.settings.edge_step)
        {
            for (x = std::max(context.state.tlogoMinX, scan.minimum_x); x <= std::min(context.state.tlogoMaxX, scan.maximum_x); x += context.settings.edge_step)
            {
                index = y * context.state.width + x;
                if (context.state.thoriz_edgemask[index])
                {
                    if (edge_tests.horizontal2(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (context.state.tvert_edgemask[index])
                {
                    if (edge_tests.vertical2(testFrame,x,y))
                    {
                        goodEdges++;
                    }

                    testEdges++;
                }
            }
        }
    }
    else if (context.settings.aggressive_logo_rejection == 3)
    {
        for (y = std::max(context.state.tlogoMinY, scan.minimum_y); y <= std::min(context.state.tlogoMaxY, scan.maximum_y); y += context.settings.edge_step)
        {
            for (x = std::max(context.state.tlogoMinX, scan.minimum_x); x <= std::min(context.state.tlogoMaxX, scan.maximum_x); x += context.settings.edge_step)
            {
                index = y * context.state.width + x;
                if (context.state.thoriz_edgemask[index])
                {
                    if (edge_tests.horizontal3(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (context.state.tvert_edgemask[index])
                {
                    if (edge_tests.vertical3(testFrame,x,y))
                    {
                        goodEdges++;
                    }

                    testEdges++;
                }
            }
        }
    }
    else if (context.settings.aggressive_logo_rejection == 4)
    {
        for (y = std::max(context.state.tlogoMinY, scan.minimum_y); y <= std::min(context.state.tlogoMaxY, scan.maximum_y); y += context.settings.edge_step)
        {
            for (x = std::max(context.state.tlogoMinX, scan.minimum_x); x <= std::min(context.state.tlogoMaxX, scan.maximum_x); x += context.settings.edge_step)
            {
                index = y * context.state.width + x;
                if (context.state.thoriz_edgemask[index] && testFrame[index] < 200)
                {
                    if (edge_tests.horizontal0(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (context.state.tvert_edgemask[index] && testFrame[index] < 200)
                {
                    if (edge_tests.vertical0(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
            }
        }
    }
    else
    {
        for (y = std::max(context.state.tlogoMinY, scan.minimum_y); y <= std::min(context.state.tlogoMaxY, scan.maximum_y); y += context.settings.edge_step)
        {
            for (x = std::max(context.state.tlogoMinX, scan.minimum_x); x <= std::min(context.state.tlogoMaxX, scan.maximum_x); x += context.settings.edge_step)
            {
                index = y * context.state.width + x;
                if (context.state.thoriz_edgemask[index])
                {
                    if (edge_tests.horizontal0(testFrame,x,y))
                    {
                        goodEdges++;
                    }
                    testEdges++;
                }
                if (context.state.tvert_edgemask[index])
                {
                    if (edge_tests.vertical0(testFrame,x,y))
                    {
                        goodEdges++;
                    }

                    testEdges++;
                }
            }
        }
    }
    if (testEdges == 0)
        return(0.5);
    return static_cast<double>(goodEdges) / static_cast<double>(testEdges);
}

void InitProcessLogoTest(RecordingContext& context)
{
    context.state.logo_block_count = 0;
    context.state.logoTrendCounter = 0;
    context.state.frames_with_logo = 0;
    context.state.lastLogoTest = false;
    context.state.curLogoTest = false;
}


bool ProcessLogoTest(RecordingContext& context, int framenum_real, int curLogoTest, int close)
{
    const auto shrink = comskip::detection::logo_shrink(context.settings.shrink_logo,
        context.settings.shrink_logo_tail, context.settings.fps);
    const auto logo_sample = comskip::detection::logo_sampling_interval(
        context.settings.fps, context.state.logoFreq);


    int i;
    double s1,s2;

    if (context.settings.logo_filter > 0)
    {
        const auto history = comskip::detection::validate_logo_filter(context.settings.logo_filter,
            logo_sample, framenum_real, context.state.frame.size());
        if (!close)
        {
            if (history.complete_windows)
            {
                s1 = s2 = 0.0;
                for (i = 0; i < context.settings.logo_filter; ++i)
                {
                    const auto offset = static_cast<std::size_t>(i) * history.sample;
                    const auto current = static_cast<std::size_t>(framenum_real);
                    s1 += (context.state.frame[current - offset - history.delay].currentGoodEdge > context.settings.logo_threshold ? 1 : -1);
                    s2 += (context.state.frame[current - offset].currentGoodEdge > context.settings.logo_threshold ? 1 : -1);
                }
                s1 /= context.settings.logo_filter;
                s2 /= context.settings.logo_filter;
                for (i = 0; i < history.sample; ++i)
                    context.state.frame[framenum_real - history.delay - i].logo_filter = s1 + s2;
            }
            for (auto index = static_cast<std::size_t>(history.recent_begin);
                 index <= static_cast<std::size_t>(framenum_real); ++index)
                context.state.frame[index].logo_filter = 0.0;
            framenum_real = history.delayed_frame;
            curLogoTest = context.state.frame[framenum_real].logo_filter > 0.0;
        }
        else
            curLogoTest = false;
    }
    if (curLogoTest != context.state.lastLogoTest)
    {
        if (!curLogoTest)
        {
            // Logo disappeared
            if (context.state.framearray && (framenum_real < 0 ||
                static_cast<std::size_t>(framenum_real) >= context.state.frame.size()))
                throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::logo_closure_exceeds_owned_frame_storage);
            const auto closed = comskip::detection::close_logo_block(
                context.state.logo_block[context.state.logo_block_count].start, framenum_real,
                comskip::detection::logo_sampling_interval(context.settings.fps, context.state.logoFreq),
                context.state.frames_with_logo, shrink);
            context.state.lastLogoTest = false;
            context.state.logoTrendCounter = 0;
            context.state.logo_block[context.state.logo_block_count].end = closed.end;
            if (closed.retained)
            {
                context.state.logo_block[context.state.logo_block_count].start = closed.start;
                context.state.frames_with_logo = closed.frames_with_logo;
                if (context.state.framearray)
                {
                    i = context.state.logo_block[context.state.logo_block_count].end;
                    if (i<0) i = 0;
                    for (; i < framenum_real; i++)
                        context.state.frame[i].logo_present = false;
                }
                LogoDebug(context, 3, "logo_block_end", std::format("{}", context.state.logo_block_count),
                    std::format("{}", context.state.logo_block[context.state.logo_block_count].end),
                    dblSecondsToStrMinutes(frame_duration(context, context.state.logo_block[context.state.logo_block_count].end,
                        context.state.logo_block[context.state.logo_block_count].start)));
                context.state.logo_block_count++;
                InitializeLogoBlockArray(context,  context.state.logo_block_count);
            }
            else
            {
                context.state.logo_block[context.state.logo_block_count].start = -1; // else discard logo cblock
            }
        }
        else
        {
            // real change or false change?
            const int next_trend = comskip::detection::add_logo_frames(context.state.logoTrendCounter, 1);
            if (next_trend == context.state.minHitsForTrend)
            {
                const auto started = comskip::detection::start_logo_block(framenum_real,
                    comskip::detection::logo_sampling_interval(context.settings.fps, context.state.logoFreq),
                    context.state.minHitsForTrend, context.state.frames_with_logo);
                if (context.state.framearray && (framenum_real < 0 ||
                    static_cast<std::size_t>(framenum_real) >= context.state.frame.size()))
                    throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::logo_appearance_exceeds_owned_frame_storage);
                InitializeLogoBlockArray(context, context.state.logo_block_count + 2);
                context.state.lastLogoTest = true;
                context.state.logoTrendCounter = 0;
                context.state.logo_block[context.state.logo_block_count + 1].start = -1;
                context.state.logo_block[context.state.logo_block_count].start = started.start;
                context.state.frames_with_logo = started.frames_with_logo;
                if (context.state.framearray)
                {
                    for (i = context.state.logo_block[context.state.logo_block_count].start; i < framenum_real; i++)
                        context.state.frame[i].logo_present = true;
                }
                if (!context.state.logo_block_count)
                {
                    LogoDebug(context, 3, "logo_block_start", std::format("{}", context.state.logo_block_count),
                        std::format("{}", context.state.logo_block[context.state.logo_block_count].start));
                }
                else
                {
                    LogoDebug(context, 3, "logo_block_start_after_gap",
                        dblSecondsToStrMinutes(frame_duration(context, context.state.logo_block[context.state.logo_block_count].start, context.state.logo_block[context.state.logo_block_count - 1].end)),
                        std::format("{}", context.state.logo_block_count),
                        std::format("{}", context.state.logo_block[context.state.logo_block_count].start));
                }
            }
            else
                context.state.logoTrendCounter = next_trend;
        }
    }
    else
    {
        context.state.logoTrendCounter = 0;
    }


    return(context.state.lastLogoTest);
}


void ResetLogoBuffers(RecordingContext& context)
{
    context.state.newestLogoBuffer = context.state.oldestLogoBuffer = 0;
    if (!context.state.logoFrameNum.empty()) {
        if (*context.state.newestLogoBuffer == context.settings.num_logo_buffers) *context.state.newestLogoBuffer = 0; // rotates buffer
        context.state.logoFrameNum[*context.state.newestLogoBuffer] = context.state.framenum_real;
        context.state.oldestLogoBuffer = 0;
    }
}

void FillLogoBuffer(RecordingContext& context)
{
    int i;
    if (context.state.newestLogoBuffer) {
        ++*context.state.newestLogoBuffer;
        if (*context.state.newestLogoBuffer == context.settings.num_logo_buffers) *context.state.newestLogoBuffer = 0; // rotates buffer
    } else {
        context.state.newestLogoBuffer = 0;
    }
    context.state.logoFrameNum[*context.state.newestLogoBuffer] = context.state.framenum_real;
    context.state.oldestLogoBuffer = 0;
    for (i = 0; i < context.settings.num_logo_buffers; i++)
    {
        if (context.state.logoFrameNum[i]  && context.state.logoFrameNum[i] < context.state.logoFrameNum[context.state.oldestLogoBuffer]) context.state.oldestLogoBuffer = i;
    }

    i = static_cast<int>(std::min(
        static_cast<std::size_t>(context.state.logoFrameBufferSize),
        static_cast<std::size_t>(context.state.width) * context.state.height * sizeof(context.state.frame_ptr[0])));
    std::copy_n(context.state.frame_ptr.begin(), i,
        context.state.logoFrameBuffer[*context.state.newestLogoBuffer].begin());

//	for (y = 0; y < height; y++) {
//		for (x = 0; x < width; x++) {
//			logoFrameBuffer[newestLogoBuffer][y * width + x] = frame_ptr[y * width + x];
//		}
//	}

    EdgeDetect(context, context.state.logoFrameBuffer[*context.state.newestLogoBuffer], *context.state.newestLogoBuffer);
    if ((!context.state.logoBuffersFull) && (*context.state.newestLogoBuffer == context.settings.num_logo_buffers - 1)) context.state.logoBuffersFull = true;
}

bool SearchForLogoEdges(RecordingContext& context)
{
    const auto scan = logo_scan(context);
    require_logo_buffer(context.state.hor_edgecount.size(), scan);
    require_logo_buffer(context.state.ver_edgecount.size(), scan);
    require_logo_buffer(context.state.thoriz_edgemask.size(), scan);
    require_logo_buffer(context.state.tvert_edgemask.size(), scan);

    int		i;
    int		x;
    int		y;
    double scale = (static_cast<double>(context.state.height) / 572) *
        (static_cast<double>(context.state.videowidth) / 720);
    double	logoPercentageOfScreen;
    bool	LogoIsThere;
    int		sum;
    int		tempMinX;
    int		tempMaxX;
    int		tempMinY;
    int		tempMaxY;
    int		last_non_logo_frame;
    int		logoFound = false;
    context.state.tlogoMinX = context.settings.edge_radius + context.settings.border;
    context.state.tlogoMaxX = context.state.videowidth - context.settings.edge_radius - context.settings.border;
    context.state.tlogoMinY = context.settings.edge_radius + context.settings.border;
    context.state.tlogoMaxY = context.state.height - context.settings.edge_radius - context.settings.border;
    std::ranges::fill(context.state.thoriz_edgemask, 0);
    std::ranges::fill(context.state.tvert_edgemask, 0);
//	minY = (logo_at_bottom ? height/2 : edge_radius + (int)(height * borderIgnore));
//	if (framearray) minY = std::max(minY, frame[frame_count].minY);
//	maxY = (subtitles? height/2 : height - edge_radius - (int)(height * borderIgnore));
//	if (framearray) maxY = std::min(maxY, frame[frame_count].maxY);

    for (const auto x : scan.columns)
    {
        for (const auto y : scan.rows) {
//	for (y = minY; y < maxY; y++) {
//		for (x = edge_radius + (int)(width * borderIgnore); x < videowidth - edge_radius + (int)(width * borderIgnore); x++) {
            if (context.state.hor_edgecount[y * context.state.width + x] >= context.settings.num_logo_buffers * 0.95 )
            {
                context.state.thoriz_edgemask[y * context.state.width + x] = 1;
            }
            if (context.state.ver_edgecount[y * context.state.width + x] >= context.settings.num_logo_buffers * 0.95 )
            {
                context.state.tvert_edgemask[y * context.state.width + x] = 1;
            }
        }
    }

    ClearEdgeMaskArea(context, context.state.thoriz_edgemask, context.state.tvert_edgemask);
    ClearEdgeMaskArea(context, context.state.tvert_edgemask, context.state.thoriz_edgemask);


    SetEdgeMaskArea(context, context.state.thoriz_edgemask);
    tempMinX = context.state.tlogoMinX;
    tempMaxX = context.state.tlogoMaxX;
    tempMinY = context.state.tlogoMinY;
    tempMaxY = context.state.tlogoMaxY;
    context.state.tlogoMinX = context.settings.edge_radius + context.settings.border;
    context.state.tlogoMaxX = context.state.videowidth - context.settings.edge_radius - context.settings.border;
    context.state.tlogoMinY = context.settings.edge_radius + context.settings.border;
    context.state.tlogoMaxY = context.state.height - context.settings.edge_radius - context.settings.border;
    SetEdgeMaskArea(context, context.state.tvert_edgemask);
    if (tempMinX < context.state.tlogoMinX) context.state.tlogoMinX = tempMinX;
    if (tempMaxX > context.state.tlogoMaxX) context.state.tlogoMaxX = tempMaxX;
    if (tempMinY < context.state.tlogoMinY) context.state.tlogoMinY = tempMinY;
    if (tempMaxY > context.state.tlogoMaxY) context.state.tlogoMaxY = tempMaxY;
    context.state.edgemask_filled = 1;
    logoPercentageOfScreen = static_cast<double>((context.state.tlogoMaxY - context.state.tlogoMinY) *
        (context.state.tlogoMaxX - context.state.tlogoMinX)) /
        static_cast<double>(context.state.height * context.state.width);
    if (logoPercentageOfScreen > context.settings.logo_max_percentage_of_screen)
    {
//			Debug(
//				3,
//				"Reducing logo search area!\tPercentage of screen - %.2f%% TOO BIG.\n",
//				logoPercentageOfScreen * 100
//			);

//        if (tempMinX > tlogoMinX+50) tlogoMinX = tempMinX;
//        if (tempMaxX < tlogoMaxX-50) tlogoMaxX = tempMaxX;
//        if (tempMinY > tlogoMinY+50) tlogoMinY = tempMinY;
//        if (tempMaxY < tlogoMaxY-50) tlogoMaxY = tempMaxY;
    }

    i = CountEdgePixels(context);
//printf("Edges=%d\n",i);
//	if (i > 350/(lowres+1)/(edge_step)) {
    if ( i > 150 * scale /context.settings.edge_step)
    {
        logoPercentageOfScreen = static_cast<double>((context.state.tlogoMaxY - context.state.tlogoMinY) *
            (context.state.tlogoMaxX - context.state.tlogoMinX)) /
            static_cast<double>(context.state.height * context.state.width);
        if (i > 40000 || logoPercentageOfScreen > context.settings.logo_max_percentage_of_screen)
        {
            LogoDebug(context, 3, "logo_edge_too_big", std::format("{}", i),
                std::format("{:.2f}", logoPercentageOfScreen * 100));
//			logoInfoAvailable = false;
        }
        else
        {
            LogoDebug(context, 3, "logo_edge_check", std::format("{}", i),
                std::format("{:.2f}", logoPercentageOfScreen * 100),
                std::format("{}", context.state.doublCheckLogoCount));
//			logoInfoAvailable = true;
            logoFound = true;
        }
    }
    else
        LogoDebug(context, 3, "logo_edge_not_enough", std::format("{}", i));


    if (logoFound)
    {
        context.state.doublCheckLogoCount++;
        LogoDebug(context, 3, "logo_double_check", std::format("{}", context.state.doublCheckLogoCount));

        if (context.state.doublCheckLogoCount > 1)
        {
            // Final check done, found
        }
        else
            logoFound = false;
    }
    else
    {
        context.state.doublCheckLogoCount = 0;
    }


    sum = 0;
    context.state.oldestLogoBuffer = 0;
    for (i = 0; i < context.settings.num_logo_buffers; i++)
    {
        if (context.state.logoFrameNum[i]  && context.state.logoFrameNum[i] < context.state.logoFrameNum[context.state.oldestLogoBuffer]) context.state.oldestLogoBuffer = i;
    }
    last_non_logo_frame = context.state.logoFrameNum[context.state.oldestLogoBuffer];
    if (logoFound)
    {
        LogoDebug(context, 3, "logo_double_check_frames",
            std::format("{}", context.state.logoFrameNum[context.state.oldestLogoBuffer]),
            std::format("{}", context.state.logoFrameNum[*context.state.newestLogoBuffer]));
        for (i = 0; i < context.settings.num_logo_buffers; i++)
        {
            context.state.currentGoodEdge = DoubleCheckStationLogoEdge(context, context.state.logoFrameBuffer[i].data());
            LogoIsThere = (context.state.currentGoodEdge > context.settings.logo_threshold);

            for (x = context.state.logoFrameNum[i]; x < context.state.logoFrameNum[i] + static_cast<int>(context.state.logoFreq * context.settings.fps); x++)
            {
                context.state.frame[x].currentGoodEdge = context.state.currentGoodEdge;
                context.state.frame[x].logo_present = LogoIsThere;
                if (!LogoIsThere)
                {
                    if (x > last_non_logo_frame)
                        last_non_logo_frame = x;
                }
            }
            if (LogoIsThere)
            {
//				Debug(7, "Logo present in frame %i.\n", logoFrameNum[i]);
                sum++;
            }
            else
            {
                LogoDebug(context, 7, "logo_not_present", std::format("{}", context.state.logoFrameNum[i]));
            }
        }
    }


    if (logoFound && (sum >= static_cast<int>(context.settings.num_logo_buffers * .9)))
    {

        context.state.clogoMinX = context.state.tlogoMinX;
        context.state.clogoMaxX = context.state.tlogoMaxX;
        context.state.clogoMinY = context.state.tlogoMinY;
        context.state.clogoMaxY = context.state.tlogoMaxY;
        std::ranges::copy(context.state.thoriz_edgemask, context.state.choriz_edgemask.begin());
        std::ranges::copy(context.state.tvert_edgemask, context.state.cvert_edgemask.begin());


        context.state.logoTrendCounter = context.settings.num_logo_buffers;
        context.state.lastLogoTest = true;
        context.state.curLogoTest = true;

        context.state.logo_block[context.state.logo_block_count].start = last_non_logo_frame+1;
        DumpEdgeMasks(context);
//		DumpEdgeMask(choriz_edgemask, HORIZ);
//		DumpEdgeMask(cvert_edgemask, VERT);
//		for (i = 0; i < num_logo_buffers; i++) {
//			free(logoFrameBuffer[i]);
//		}
//		free(horiz_count);
//		horiz_count = NULL;
//		free(vert_count);
//		vert_count = NULL;
//		free(logoFrameBuffer);
//		logoFrameBuffer = NULL;
        InitScanLines(context);
        InitHasLogo(context);

        context.state.logoInfoAvailable = true; //xxxxxxx
    }
    else
    {
//		logoInfoAvailable = false; //xxxxxxx
        context.state.currentGoodEdge = 0.0;
    }

    if (!context.state.logoInfoAvailable && context.settings.startOverAfterLogoInfoAvail &&
        (context.state.framenum_real > static_cast<int>(context.settings.giveUpOnLogoSearch * context.settings.fps)))
    {
        Debug(context, 1, context.translator.format("detection_no_logo", context.state.framenum_real));
        comskip::detection::disable_method(context.settings.commDetectMethod, comskip::detection::DetectionMethod::logo);
    }
    if (context.settings.added_recording > 0)
        context.settings.giveUpOnLogoSearch += context.settings.added_recording * 60;

    if (context.state.logoInfoAvailable && context.settings.startOverAfterLogoInfoAvail)
    {
        LogoDebug(context, 3, "logo_found_bounds", std::format("{}", context.state.framenum_real),
            std::format("{}", context.state.clogoMinX), std::format("{}", context.state.clogoMaxX),
            std::format("{}", context.state.clogoMinY), std::format("{}", context.state.clogoMaxY));
        SaveLogoMaskData(context);
        LogoDebug(context, 3, "logo_processing_end");
        return false;
    }

    return true;
}


int ClearEdgeMaskArea(RecordingContext& context, std::span<unsigned char> temp,
                      std::span<const unsigned char> test)
{
    const auto scan = logo_scan(context);
    const auto required = static_cast<std::size_t>(context.state.width) * context.state.height;
    if (temp.size() < required || test.size() < required)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::logo_mask_cleanup_requires_both_pixel_buffers);

    int x;
    int y;
    int count;
    int valid = 0;
    int offset;
    int ix,iy;

    for (const auto x : scan.columns)
    {
        for (const auto y : scan.rows)
        {
            count = 0;
            if (temp[y * context.state.width + x] == 1)
            {
                bool found = false;
                if (test[y * context.state.width + x] == 1)
//					goto found;
                    count++;

                for (offset = context.settings.edge_step; !found && offset < static_cast<int>(maximum_edge_search_fraction * context.state.width); offset += context.settings.edge_step)
                {
                    iy = std::min(y+offset,context.state.height-1);
                    for (ix= std::max(x-offset,0); ix <= std::min(x+offset, context.state.width-1); ix += context.settings.edge_step)
                        if (test[iy * context.state.width + ix] == 1)
//							goto found;
                            count++;

                    iy = std::max(y-offset,0);
                    for (ix= std::max(x-offset,0); ix <= std::min(x+offset, context.state.width-1); ix += context.settings.edge_step)
                        if (test[iy * context.state.width + ix] == 1)
//							goto found;
                            count++;

                    ix = std::min(x+offset, context.state.width-1);
                    for (iy= std::max(y-offset+context.settings.edge_step,0); iy <=  std::min(y+offset-context.settings.edge_step,context.state.height-1); iy += context.settings.edge_step)
                        if (test[iy * context.state.width + ix] == 1)
//							goto found;
                            count++;

                    ix = std::max(x-offset,0);
                    for (iy= std::max(y-offset+context.settings.edge_step,0); iy <=  std::min(y+offset-context.settings.edge_step,context.state.height-1); iy += context.settings.edge_step)
                        if (test[iy * context.state.width + ix] == 1)
//							goto found;
                            count++;
                    if (count >= context.settings.edge_weight)
                    {
                        found = true;
                        break;
                    }
                }
                if (found)
                    ++valid;
                else
                    temp[y * context.state.width + x] = 0;
            }
        }
    }
    return(valid);
}

void SetEdgeMaskArea(RecordingContext& context, std::span<const unsigned char> temp)
{
    const auto scan = logo_scan(context);
    const auto required = static_cast<std::size_t>(context.state.width) * context.state.height;
    if (temp.size() < required)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::logo_mask_bounds_require_mask_pixels);

    int x;
    int y;
    context.state.tlogoMinX = context.state.videowidth - 1;
    context.state.tlogoMaxX = 0;
    context.state.tlogoMinY = context.state.height - 1;
    context.state.tlogoMaxY = 0;
    for (const auto x : scan.columns)
//    for (y = (logo_at_bottom ? height/2 : border + edge_radius); y < (subtitles? height/2 : height - border - edge_radius); y++)
    {
        for (const auto y : scan.rows)
//        for (x = border+edge_radius; x < videowidth - border - edge_radius; x++)
        {
            if (temp[y * context.state.width + x] == 1)
            {
                if (x - scan.expansion < context.state.tlogoMinX) context.state.tlogoMinX = x - scan.expansion;
                if (y - scan.expansion < context.state.tlogoMinY) context.state.tlogoMinY = y - scan.expansion;
                if (x + scan.expansion > context.state.tlogoMaxX) context.state.tlogoMaxX = x + scan.expansion;
                if (y + scan.expansion > context.state.tlogoMaxY) context.state.tlogoMaxY = y + scan.expansion;
            }
        }
    }

    if (context.state.tlogoMinX < context.settings.edge_radius) context.state.tlogoMinX = context.settings.edge_radius;
    if (context.state.tlogoMaxX > (context.state.videowidth - context.settings.edge_radius)) context.state.tlogoMaxX = (context.state.videowidth - context.settings.edge_radius);
    if (context.state.tlogoMinY < context.settings.edge_radius) context.state.tlogoMinY = context.settings.edge_radius;
    if (context.state.tlogoMaxY > (context.state.height - context.settings.edge_radius)) context.state.tlogoMaxY = (context.state.height - context.settings.edge_radius);
}

int CountEdgePixels(RecordingContext& context)
{
    const auto scan = logo_scan(context);
    require_logo_buffer(context.state.thoriz_edgemask.size(), scan);
    require_logo_buffer(context.state.tvert_edgemask.size(), scan);
    int x;
    int y;
    int count = 0;
    int hcount = 0;
    int vcount = 0;
    for (y = std::max(context.state.tlogoMinY, scan.minimum_y); y <= std::min(context.state.tlogoMaxY, scan.maximum_y); y++)
    {
        for (x = std::max(context.state.tlogoMinX, scan.minimum_x); x <= std::min(context.state.tlogoMaxX, scan.maximum_x); x++)
        {
            if (context.state.thoriz_edgemask[y * context.state.width + x]) hcount++;
            if (context.state.tvert_edgemask[y * context.state.width + x]) vcount++;
        }
    }
    count = hcount + vcount;
//    if (count>0)
//        Debug(1, "\nFrame[%d] edgecount=%d",framenum_real, count);
//	printf("%6d %6d\n",hcount, vcount);
    //if ((hcount < 50 * scale / edge_step) || (vcount < 50 * scale /edge_step )) count = 0;
    return (count);
}

void DumpEdgeMask(RecordingContext& context, std::span<const unsigned char> buffer, int direction)
{
    int x;
    int y;
    std::vector<char> outbuf(static_cast<std::size_t>(
        std::max(0, context.state.clogoMaxX - context.state.clogoMinX + 1)) + 1);
    switch (direction)
    {
    case horizontal_edge_direction:
        LogoDebug(context, 1, "logo_mask_heading", context.translator.text("logo_mask_horizontal"));
        break;

    case vertical_edge_direction:
        LogoDebug(context, 1, "logo_mask_heading", context.translator.text("logo_mask_vertical"));
        break;

    case diagonal_1_edge_direction:
        LogoDebug(context, 1, "logo_mask_heading", context.translator.text("logo_mask_diagonal_1"));
        break;

    case diagonal_2_edge_direction:
        LogoDebug(context, 1, "logo_mask_heading", context.translator.text("logo_mask_diagonal_2"));
        break;
    }

    for (x = context.state.clogoMinX; x <= context.state.clogoMaxX; x++)
    {
        outbuf[x-context.state.clogoMinX] = '0'+ (x % 10);
    }
    outbuf[x-context.state.clogoMinX] = 0;
    Debug(context, 1, "%s\n",outbuf.data());


    Debug(context, 1, "\n");
    for (y = context.state.clogoMinY; y <= context.state.clogoMaxY; y++)
    {
        Debug(context, 1, "%3d: ", y);
        for (x = context.state.clogoMinX; x <= context.state.clogoMaxX; x++)
        {
            switch (buffer[y * context.state.width + x])
            {
            case 0:
                outbuf[x-context.state.clogoMinX] = ' ';
                break;

            case 1:
                outbuf[x-context.state.clogoMinX] = '*';
                break;
            }
        }
        outbuf[x-context.state.clogoMinX] = 0;
        Debug(context, 1, "%s\n",outbuf.data());

    }
}

void DumpEdgeMasks(RecordingContext& context)
{
    int x;
    int y;
    std::vector<char> outbuf(static_cast<std::size_t>(
        std::max(0, context.state.clogoMaxX - context.state.clogoMinX + 1)) + 1);

    for (x = context.state.clogoMinX; x <= context.state.clogoMaxX; x++)
    {
        outbuf[x-context.state.clogoMinX] = '0'+ (x % 10);
    }
    outbuf[x-context.state.clogoMinX] = 0;
    Debug(context, 1, "%s\n",outbuf.data());

    for (y = context.state.clogoMinY; y <= context.state.clogoMaxY; y++)
    {
        Debug(context, 1, "%3d: ", y);
        for (x = context.state.clogoMinX; x <= context.state.clogoMaxX; x++)
        {
            switch (context.state.choriz_edgemask[y * context.state.width + x])
            {
            case 0:
                if (context.state.cvert_edgemask[y * context.state.width + x] == 1)
                    outbuf[x-context.state.clogoMinX] =  '-';
                else
                    outbuf[x-context.state.clogoMinX] =  ' ';
                break;

            case 1:
                if (context.state.cvert_edgemask[y * context.state.width + x] == 1)
                    outbuf[x-context.state.clogoMinX] =  '+';
                else
                    outbuf[x-context.state.clogoMinX] =  '|';
                break;
            }
        }
        outbuf[x-context.state.clogoMinX] = 0;
        Debug(context, 1, "%s\n",outbuf.data());
    }
}

bool CheckFramesForLogo(RecordingContext& context, int start, int end)
{
    int		i;
    double sum = 0.0;
    for (i = start; i <= end; i++)
        sum += (context.state.frame[i].currentGoodEdge > context.settings.logo_threshold ? 1 : 0);

    sum = sum / (end - start + 1);
    if (sum > context.settings.logo_percentage_threshold)
        return(true);
    return(false);


}

double CalculateLogoFraction(RecordingContext& context, int start, int end)
{
    int		i,j;
    int		count=0;
    j = 0;
    for (i = start; i <= end; i++)
    {
        while (j < context.state.logo_block_count && i > context.state.logo_block[j].end) j++;
        if (j < context.state.logo_block_count && i >= context.state.logo_block[j].start && i <= context.state.logo_block[j].end )
            count++;
    }
    if (context.state.reverseLogoLogic)
        return 1.0 - static_cast<double>(count) / static_cast<double>(end - start + 1);
    return static_cast<double>(count) / static_cast<double>(end - start + 1);
}

bool CheckFrameForLogo(RecordingContext& context, int i)
{
    int		j=0;
    while (j < context.state.logo_block_count && i > context.state.logo_block[j].end) j++;
    if (j < context.state.logo_block_count && i <= context.state.logo_block[j].end && i >= context.state.logo_block[j].start )
    {
        return(!context.state.reverseLogoLogic);
    }
    return (context.state.reverseLogoLogic);
}


char CheckFramesForCommercial(RecordingContext& context, int start, int end)
{
    int		i;
    if (start >= end )
        return ('0');						// Too short to decide
    i = 0;
    while (i <= context.state.commercial_count && start > context.state.commercial[i].end_frame)
        i++;
    if (i <= context.state.commercial_count)  			// Now start <= commercial[i].end_frame
    {
        if (end < context.state.commercial[i].start_frame)
            return('+');
        if (start < context.state.commercial[i].start_frame)
            return('0');
        return('-');
    }
    return('+');
}

char CheckFramesForReffer(RecordingContext& context, int start, int end)
{
    int		i;
    if (context.state.reffer_count < 0)
        return(' ');
    if (start >= end )
        return ('0');						// Too short to decide
    i = 0;
    while (i <= context.state.reffer_count &&  context.state.reffer[i].end_frame < start + context.settings.fps)
        i++;
    if (i <= context.state.reffer_count)  			// Now start <= reffer[i].end_frame
    {
        if (context.state.reffer[i].start_frame < start + context.settings.fps)
            return('-');
        if (context.state.reffer[i].start_frame > end - context.settings.fps)
            return('+');
        if ( context.state.reffer[i].start_frame < end + context.settings.fps)
            return('0');
        return('-');
    }
    return('+');
}

void SaveLogoMaskData(RecordingContext& context)
{
    auto logo_file=comskip::platform::open_file_owned(context.state.logofilename, "w");
    if (!logo_file)
        throw comskip::diagnostics::DiagnosticError<std::runtime_error>(
            comskip::diagnostics::Code::output_open,{context.state.logofilename});
    comskip::detection::write_saved_logo(*logo_file,
        {context.state.width,context.state.height,context.state.clogoMinX,context.state.clogoMaxX,
         context.state.clogoMinY,context.state.clogoMaxY},context.state.choriz_edgemask,
        context.state.cvert_edgemask,context.state.logofilename);
    comskip::output::checked_close(logo_file, context.state.logofilename,
        comskip::diagnostics::Code::output_write);
}

void LoadLogoMaskData(RecordingContext& context)
{
    comskip::platform::FilePtr txt_file;
    std::array<char, 2000> data{};
    char* ptr = nullptr;
    long tmpLong = 0;
    auto logo_file = comskip::platform::open_file_owned(context.state.logofilename, "rb");
    if (!logo_file) {
        Debug(context, 0, context.translator.text("detection_logo_file_missing"));
        context.state.logoInfoAvailable = false;
        return;
    }
    LogoDebug(context, 1, "logo_using_data", context.state.logofilename);
    auto loaded = comskip::detection::read_saved_logo(*logo_file,
        {context.state.width, context.state.height, context.state.clogoMinX, context.state.clogoMaxX,
         context.state.clogoMinY, context.state.clogoMaxY},
        {context.settings.edge_radius, context.settings.edge_step, context.settings.border,
         context.settings.logo_at_side != 0, context.settings.logo_at_bottom != 0, context.settings.subtitles != 0},
        maximum_saved_logo_width, maximum_saved_logo_height);
    comskip::output::checked_close(logo_file, context.state.logofilename,
        comskip::diagnostics::Code::cannot_read_saved_logo);
    context.state.videowidth = context.state.width = loaded.geometry.width;
    context.state.height = loaded.geometry.height;
    context.state.ensure_pixel_buffers(true);
    context.state.clogoMinX = loaded.geometry.minimum_x;
    context.state.clogoMaxX = loaded.geometry.maximum_x;
    context.state.clogoMinY = loaded.geometry.minimum_y;
    context.state.clogoMaxY = loaded.geometry.maximum_y;
    context.state.choriz_edgemask = std::move(loaded.horizontal);
    context.state.cvert_edgemask = std::move(loaded.vertical);

    context.state.logoInfoAvailable = true;
    context.settings.startOverAfterLogoInfoAvail = true; // prevent continuous searching for logo when a logo file is specified
    context.state.secondLogoSearch = true;
    InitScanLines(context);
    InitHasLogo(context);
    context.state.isSecondPass = true;
    if (!context.state.loadingCSV)
    {
//		DumpEdgeMask(choriz_edgemask, HORIZ);
//		DumpEdgeMask(cvert_edgemask, VERT);
        DumpEdgeMasks(context);
    }
    std::ranges::fill(data, 0);
    std::fflush(nullptr);
    if (context.settings.output_default)
    {
        txt_file = comskip::platform::open_file_owned(context.state.out_filename, "r");
        if (!txt_file)
        {
            sleep_for_ms(50L);
            txt_file = comskip::platform::open_file_owned(context.state.out_filename, "r");
            if (!txt_file)
            {
                Debug(context, 0, context.translator.format("detection_output_read_failed", context.state.out_filename));
                context.state.isSecondPass = false;
                return;
            }
        }


        if(fseek( txt_file.get(), 0L, SEEK_SET ))
        {
            Debug(context, 0, context.translator.text("detection_output_seek_failed"));
        }


        while (fgets(data.data(), static_cast<int>(data.size() - 1), txt_file.get()) != nullptr)
        {
            if (strstr(data.data(), "FILE PROCESSING COMPLETE") != nullptr)
            {
                context.state.lastFrame = 0;
                break;
            }
            ptr = strchr(data.data(), '\t');
            if (ptr != nullptr)
            {
                ptr++;
                tmpLong = strtol(ptr, nullptr, 10);
                if (tmpLong > context.state.lastFrame)
                {
                    context.state.lastFrame = tmpLong;
                }
            }
        }
        if (std::ferror(txt_file.get()))
            throw comskip::diagnostics::DiagnosticError<std::runtime_error>(
                comskip::diagnostics::Code::cannot_read_detection_output,{context.state.out_filename});
        comskip::output::checked_close(txt_file, context.state.out_filename,
            comskip::diagnostics::Code::cannot_read_detection_output);
    }
    LogoDebug(context, 10, "logo_last_frame", context.state.out_filename,
        std::format("{}", context.state.lastFrame));
}
