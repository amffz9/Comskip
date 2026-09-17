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
#include "app/debug.h"
#include "detection/frame_timestamps.h"
#include "media/decoder.h"
#include "media/audio_analysis.h"
#include "media/timing_diagnostics.h"
#include "output/selftest_log.h"
#include "localization/diagnostic.h"
#include "platform.h"
#include "comskip.h"
#include "ffmpeg_resources.h"
#include "checked_format.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
using namespace comskip::media;
#define SELFTEST

namespace {
std::string ffmpeg_detail(int status) {
    std::array<char,AV_ERROR_MAX_STRING_SIZE> detail{};
    av_strerror(status,detail.data(),detail.size());
    return detail.data();
}
void file_open_impl(RecordingContext& context)
{
    VideoState *is;
    int subtitle_index= -1, audio_index= -1, video_index = -1;
    int openretries = 0;

    if (context.state.video_owner.get() == NULL)
    {
        context.state.video_owner = std::make_unique<VideoState>();
        is = context.state.video_owner.get();
        // Register all formats and codecs
        context.state.av_log_level=AV_LOG_INFO;


        av_log_set_flags(AV_LOG_SKIP_REPEATED);

        is->videoStream=-1;
        is->audioStream=-1;
        is->subtitleStream = -1;
        is->pFormatCtx.reset();

//        av_dict_set_int(&opts, "lowres", stream_lowres, 0);
        if (!context.settings.hardware_decode) {
//            codecCtx->flags |= AV_CODEC_FLAG_GRAY;
            av_dict_set_int(std::inout_ptr(context.state.myoptions), "gray", 1, 0);
        }
#ifdef DONATOR
//        if (thread_count == 1)
                av_dict_set_int(std::inout_ptr(context.state.myoptions), "threads", context.settings.thread_count, 0);
//        else
//            av_dict_set(std::inout_ptr(myoptions), "threads", "auto", 0);
//           codecCtx->thread_count= thread_count;
#else
            av_dict_set_int(std::inout_ptr(context.state.myoptions), "threads", 1, 0);
//            codecCtx->thread_count= 1;
#endif
        av_dict_set_int(std::inout_ptr(context.state.myoptions), "refcounted_frames", 1, 0); // No need to keep multiple buffers


    }
    else
        is = context.state.video_owner.get();
    // Open video file
    if ( is->pFormatCtx.get() == NULL)
    {
        is->filename = context.state.mpegfilename;
        is->pFormatCtx.reset(avformat_alloc_context());
        if (!is->pFormatCtx) throw std::bad_alloc();
        is->pFormatCtx->max_analyze_duration *= 4;
//        pFormatCtx->probesize = 400000;
again:
        const int open_status=avformat_open_input(std::inout_ptr(is->pFormatCtx), is->filename.c_str(), NULL,std::inout_ptr(context.state.myoptions));
        if(open_status<0)
        {
            if (openretries++ < context.settings.live_tv_retries)
            {
                sleep_for_ms(1000L);
                goto again;
            }
            throw comskip::diagnostics::DiagnosticError<std::runtime_error>(
                comskip::diagnostics::Code::cannot_open_recording_detail,
                {is->filename,ffmpeg_detail(open_status)});

        }
        is->seek_by_bytes = !!(is->pFormatCtx->iformat->flags & AVFMT_TS_DISCONT) && strcmp("ogg", is->pFormatCtx->iformat->name);
// #if def _DEBUG
//        if (is->duration < 5*60 && retries++ < live_tv_retries)
//        {
//            sleep_for_ms(4000L);
//            goto again;
//        }
// #en dif
//     is->pFormatCtx->max_analyze_duration = 320000000;
//    is->pFormatCtx->thread_count= 2;

        // Retrieve stream information
        const int stream_info_status=avformat_find_stream_info(is->pFormatCtx.get(), 0L );
        if(stream_info_status<0)
        {
            throw comskip::diagnostics::DiagnosticError<std::runtime_error>(
                comskip::diagnostics::Code::cannot_read_recording_stream_info_detail,
                {is->filename,ffmpeg_detail(stream_info_status)});
        }
        // Dump information about file onto standard error
        if (context.state.retries == 0) av_dump_format(is->pFormatCtx.get(), 0, is->filename.c_str(), 0);
    }

    if (!is->frame.get()) {
        is->frame = make_frame();
    }

    if ( is->videoStream == -1)
    {
        video_index = av_find_best_stream(is->pFormatCtx.get(), AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
        if(video_index >= 0)
        {
            stream_component_open(context, is, video_index);
        }
        if(is->videoStream < 0)
        {
            throw comskip::diagnostics::DiagnosticError<std::runtime_error>(
                comskip::diagnostics::Code::recording_has_no_decodable_video_stream,{is->filename});
        }

        if ( is->video_st->duration == AV_NOPTS_VALUE ||  is->video_st->duration < 0)
            is->duration =  ((float)is->pFormatCtx->duration) / AV_TIME_BASE;
        else
            is->duration =  av_q2d(is->video_st->time_base)* is->video_st->duration;

        if (is->duration < 0 && (context.settings.live_tv_retries > 0)) {
           Debug(context, 0, "%s", context.translator.text("media_duration_warning"));
        }


        /* Calc FPS */
        if(is->video_st->r_frame_rate.den && is->video_st->r_frame_rate.num)
        {
            is->fps = av_q2d(is->video_st->r_frame_rate);
        }
        else
        {
            Debug(context, 10, "%s", context.translator.text("media_no_stream_frame_rate"));
            is->fps = 1/(av_q2d(is->dec_ctx->time_base) * is->ticks_per_frame );
        }
        set_fps(context,  1.0 / is->fps);
//        Debug(1, "Stream frame rate is %5.3f f/s\n", is->fps);


    }

    if (is->audioStream== -1 && video_index>=0)
    {

        audio_index = av_find_best_stream(is->pFormatCtx.get(), AVMEDIA_TYPE_AUDIO, -1, video_index, NULL, 0);
        if(audio_index >= 0)
        {
            stream_component_open(context, is, audio_index);
            if (is->audio_st)
                context.state.audio_channels = is->audio_st->codecpar->ch_layout.nb_channels;

            if (is->audioStream < 0)
            {
                Debug(context, 1, "%s", context.translator.text("media_audio_decoder_warning"));
            }
        }

    }

    if (is->subtitleStream == -1 && video_index>=0)
    {
        subtitle_index = av_find_best_stream(is->pFormatCtx.get(), AVMEDIA_TYPE_SUBTITLE, -1, video_index, NULL, 0);
        if(subtitle_index >= 0)
        {
            is->subtitleStream = subtitle_index;
            is->subtitle_st = is->pFormatCtx->streams[subtitle_index];
            if (context.captions && !context.state.reviewing)
                context.captions->select_stream(*is->subtitle_st->codecpar, is->subtitle_st->time_base);
            if (context.state.demux_pid)
                context.state.selected_subtitle_pid = is->subtitle_st->id;
        }

    }
    context.state.av_log_level=AV_LOG_ERROR;


                    is->seek_req = 0;
//                    framenum = 0;
                    context.state.pts_offset = 0.0;
                    is->video_clock = 0.0;
                    is->audio_clock = 0.0;
//                    sound_frame_counter = 0;
//                    initial_pts = 0.0;
//                    initial_pts_set = 0;
//                    initial_apts_set = 0;
                    context.state.initial_apts = 0;
                    context.state.apts_offset = 0.0;
                    context.state.base_apts = 0.0;
                    context.state.top_apts = 0.0;
                    context.state.apts = 0.0;
                    context.state.audio_buffer_ptr = context.state.audio_buffer;
                    context.state.audio_samples = 0;
//                    close_data();
#ifdef PROCESS_CC
#endif

}
}

void file_open(RecordingContext& context) {
    try { file_open_impl(context); }
    catch (...) { file_close(context); throw; }
}

void file_close(RecordingContext& context) {
    if (!context.state.video_owner) return;
    auto& video = *context.state.video_owner;
    video.dec_ctx.reset();
    video.audio_ctx.reset();
    video.subtitle_ctx.reset();
    video.videoStream = video.audioStream = video.subtitleStream = -1;
    // Borrowed stream references cannot outlive the input that owns them.
    video.video_st = video.audio_st = video.subtitle_st = nullptr;
    video.pFormatCtx.reset();
    video.frame.reset();
    video.pFrame.reset();
    video.img_convert_ctx.reset();
    context.state.ac3_packet_index = 0;
    context.state.ac3_package_misalignment_count = 0;
}
