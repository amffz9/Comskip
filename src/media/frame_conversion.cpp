#include "frame_conversion.h"
#include "ffmpeg_resources.h"

#include <cerrno>
extern "C" {
#include <libavutil/frame.h>
#include <libavutil/error.h>
#include <libswscale/swscale.h>
}

namespace comskip::media {
int convert_frame_to_8bit(AVFrame* frame, SwsContext*& context)
{
    if (!frame || frame->width <= 0 || frame->height <= 0)
        return AVERROR(EINVAL);
    if (frame->format != AV_PIX_FMT_YUV420P10LE)
        return AVERROR(EINVAL);
    context = sws_getCachedContext(context, frame->width, frame->height,
        AV_PIX_FMT_YUV420P10LE, frame->width, frame->height, AV_PIX_FMT_YUV420P,
        SWS_POINT, nullptr, nullptr, nullptr);
    if (!context)
        return AVERROR(ENOMEM);
    FramePtr converted(av_frame_alloc());
    if (!converted)
        return AVERROR(ENOMEM);
    int result = av_frame_copy_props(converted.get(), frame);
    if (result < 0)
        return result;
    converted->format = AV_PIX_FMT_YUV420P;
    converted->width = frame->width;
    converted->height = frame->height;
    result = av_frame_get_buffer(converted.get(), 0);
    if (result < 0)
        return result;
    result = sws_scale(context, frame->data, frame->linesize, 0, frame->height,
        converted->data, converted->linesize);
    if (result < 0)
        return result;
    if (result != frame->height)
        return AVERROR(EINVAL);
    av_frame_unref(frame);
    av_frame_move_ref(frame, converted.get());
    return 0;
}
}
