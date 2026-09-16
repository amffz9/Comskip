/*
 * mpeg2dec.c
 * Copyright (C) 2000-2003 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 * See http://libmpeg2.sourceforge.net/ for updates.
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "recording_context.h"
#include "media/decoder.h"
#include "media/audio_analysis.h"
#include "media/timing_diagnostics.h"
#include "output/selftest_log.h"
#include "exit_requested.h"
#include "platform.h"
#include "comskip.h"
#include "ffmpeg_resources.h"
#include "checked_format.h"
#include "seek_math.h"
#include "localization/diagnostic.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <memory>
using namespace comskip::media;
#define SELFTEST

void Set_seek(RecordingContext& context, VideoState *is, double pts)
{
    AVFormatContext *ic = is->pFormatCtx.get();

    double length = is->duration;

    is->seek_flags = AVSEEK_FLAG_ANY;
    is->seek_flags = AVSEEK_FLAG_BACKWARD;
    is->seek_req = true;
    is->seek_pts = pts;
#ifdef DEBUG
    fputs(context.translator.format("media_seek_target", std::format("{:8.2f}", pts)).c_str(), stdout);
#endif // DEBUG

#define MAX_GOP_SIZE 2.0
    pts = fmax(0.0,pts-MAX_GOP_SIZE);
    const auto failed=[]() -> void {
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(
            comskip::diagnostics::Code::integer_range,{"seek target"});
    };

    if (is->seek_by_bytes)
    {
//                            pos = avio_tell(is->pFormatCtx->pb);
        const auto size=avio_size(ic->pb);
        if (length <= 0 || !std::isfinite(length)) {
            const auto fallback=comskip::media::recording_duration(context.state.frame_count,get_fps(context));
            if (!fallback) failed();
            length=*fallback;
        }
        const auto position=comskip::media::byte_seek_position(size,length,fmax(0.0,pts-4.0));
        if (!position) failed();
        is->seek_pos=*position;
        is->seek_flags |= AVSEEK_FLAG_BYTE;
    } else {
        const auto position=comskip::media::timestamp_seek_position(pts,context.state.initial_pts,
            is->video_st->time_base.num,is->video_st->time_base.den,
            is->video_st->start_time==AV_NOPTS_VALUE ? std::nullopt : std::optional{is->video_st->start_time});
        if (!position) failed();
        is->seek_pos=*position;
    }
}

void DoSeekRequest(RecordingContext& context, VideoState *is)
{
    int ret;
again:
//           ret = avformat_seek_file(is->pFormatCtx.get(), is->videoStream, INT64_MIN, is->seek_pos, INT64_MAX, is->seek_flags);
    ret = av_seek_frame(is->pFormatCtx.get(), is->videoStream,  is->seek_pos,  is->seek_flags);
//            ret = av_seek_frame(is->pFormatCtx.get(), -1,  is->seek_pos,  is->seek_flags);
    context.state.pev_best_effort_timestamp = 0;
    context.state.best_effort_timestamp = 0;
    is->video_clock = 0.0;
    is->audio_clock = 0.0;
    if(ret < 0)
    {
        const char *error_text;
#if LIBAVCODEC_BUILD >= AV_VERSION_INT(59, 37, 100) && \
    LIBAVUTIL_BUILD >= AV_VERSION_INT(57, 28, 100)
        error_text = "Generic";
#else
        if (is->pFormatCtx->iformat->read_seek)
        {
            error_text = "Format specific";
        }
        else if(is->pFormatCtx->iformat->read_timestamp)
        {
            error_text = "Frame binary";
        }
        else
        {
            error_text = "Generic";
        }
#endif

        fputs(context.translator.format("media_seek_error", error_text,
              std::format("{:6.3f}", is->seek_pts), is->pFormatCtx->url).c_str(), stderr);

        if (context.state.selftest)
        {
            comskip::output::write_selftest_log(context.settings.selftest_log_file, "{} error while seeking, target={:6.3f}, \"{}\"\n", error_text,is->seek_pts, is->pFormatCtx->url);
        }

        if (!is->seek_by_bytes)
        {
            is->seek_by_bytes = 1; // Fall back to byte seek
            Set_seek(context, is, is->seek_pts);
            goto again;
        }
    }
    if (!is->seek_no_flush)
    {
        if(is->audioStream >= 0)
        {
            avcodec_flush_buffers(is->audio_ctx.get());
        }
        if(is->videoStream >= 0)
        {
            avcodec_flush_buffers(is->dec_ctx.get());
        }
    }
    is->seek_no_flush = 0;
}

void DecodeOnePicture(RecordingContext& context, FILE * f, double pts)
{
    VideoState *is = context.state.video_owner.get();
    auto packet_owner = make_packet();
    AVPacket *packet = packet_owner.get();
//    int ret;

//    int64_t pack_pts=0, comp_pts=0, pack_duration=0;

    file_open(context);
    is = context.state.video_owner.get();

    context.state.reviewing = 1;
    Set_seek(context, is, pts);

    context.state.pev_best_effort_timestamp = 0;
    context.state.best_effort_timestamp = 0;
    context.state.pts_offset = 0.0;

//     Debug ( 5,  "Seek to %f\n", pts);
    context.state.frame_ptr = NULL;

    for(;;)
    {
        if(is->quit)
        {
            break;
        }
        // seek stuff goes here
        if(is->seek_req)
        {
again:      DoSeekRequest(context, is);
        }
nextpacket:
        if(av_read_frame(is->pFormatCtx.get(), packet) < 0)
        {
            break;
        }
        if (is->seek_req) {
                double packet_time = (packet->pts - (is->video_st->start_time != AV_NOPTS_VALUE ? is->video_st->start_time : 0)) * av_q2d(is->video_st->time_base);
            if (packet->pts==AV_NOPTS_VALUE) {
                av_packet_unref(packet);
                goto nextpacket;
            }
            if (is->seek_req < 6 && (is->seek_flags & AVSEEK_FLAG_BYTE) &&  is->duration > 0 && fabs(packet_time - (is->seek_pts - 2.5) ) < is->duration / (10 * is->seek_req)) {
                is->seek_pos += ((is->seek_pts - 2.5 - packet_time) / is->duration ) * avio_size(is->pFormatCtx->pb) * 1.1;
                is->seek_req++;
                goto again;
            }
            is->seek_req = 0;
        }
        is->seek_req = 0;

        if(packet->stream_index == is->videoStream)
        {
/*
            if (packet->pts != AV_NOPTS_VALUE)
                comp_pts = packet->pts;
            pack_pts = comp_pts; // av_rescale_q(comp_pts, is->video_st->time_base, AV_TIME_BASE_Q);
            pack_duration = packet->duration; //av_rescale_q(packet->duration, is->video_st->time_base, AV_TIME_BASE_Q);
            comp_pts += packet->duration;
 */
 //           pass = 0;
            context.state.retries = 1; // once a frame has been decoded this will be set to zero
            if (video_packet_process(context, is, packet) )
            {

                if (context.state.retries == 0) // A frame has been decoded so stop reading packets.
                {
#ifdef DEBUG
    fputs(context.translator.format("media_seek_landed", std::format("{:8.2f}", is->video_clock)).c_str(), stdout);
#endif // DEBUG

                    av_packet_unref(packet);
                    break;
                }
/*
                double frame_delay = av_q2d(is->dec_ctxpar->time_base)* is->dec_ctxpar->ticks_per_frame;         // <------------------------ frame delay is the time in seconds till the next frame
                if (is->video_clock - is->seek_pts > -frame_delay / 2.0)
                {
                    av_packet_unref(packet);
                    break;
                }
                if (is->video_clock + (pack_duration * av_q2d(is->video_st->time_base)) >= is->seek_pts)
                {
                    av_packet_unref(packet);
                    break;
                }
 */
            }
        }
        else if(packet->stream_index == is->audioStream)
        {
            // audio_packet_process(is, packet);
        }
        else
        {
            // Do nothing
        }
        av_packet_unref(packet);
    }
    context.state.reviewing = 0;
}





