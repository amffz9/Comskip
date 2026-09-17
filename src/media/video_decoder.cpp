#include "output/selftest_log.h"
#include "app/debug.h"
#include "recording_context.h"
#include "detection/captions.h"
#include "detection/detection_methods.h"
#include "detection/detector_runtime.h"
#include "detection/frame_timestamps.h"
#include "detection/initialization.h"
#include "detection/logo_detection.h"
#include "media/decoder.h"
#include "media/audio_analysis.h"
#include "media/timing_diagnostics.h"
#include "output/media_dump.h"
#include "exit_requested.h"
#include "a53_caption_bridge.h"
#include "video_decode_status.h"
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

#include "platform.h"
#include "comskip.h"
#include "ffmpeg_resources.h"
#include <memory>
using namespace comskip::media;
#include "settings_value.h"
#include "translator.h"
#include <algorithm>
#include <limits>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <format>
#include <string_view>
#include <utility>
#include "checked_format.h"
#include "frame_conversion.h"
#include "input_position.h"
#include "video_timestamp.h"

namespace {

constexpr double timeline_comparison_epsilon = 0.001;

[[nodiscard]] bool approximately_equal(const double left, const double right) noexcept
{
    return std::abs(left - right) < timeline_comparison_epsilon;
}

template <typename... Args>
void debug_message(RecordingContext& context, const int level, const std::string_view message_id, Args&&... args)
{
    const auto message = context.translator.format(message_id, std::forward<Args>(args)...);
    Debug(context, level, "%s", message.c_str());
}

} // namespace

#define SELFTEST

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

//#define restrict
//#include <libavcodec/ac3dec.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
#include <libavutil/samplefmt.h>
#include <libswscale/swscale.h>
}

int convert_frame_to_8bit_owned(AVFrame* frame, ScalerPtr& context) {
    auto* raw_context = context.release();
    const int result = comskip::media::convert_frame_to_8bit(frame, raw_context);
    context.reset(raw_context);
    return result;
}

// int width, height;

//#include "mpeg2convert.h"
#include "comskip.h"
#include "ffmpeg_resources.h"
#include <memory>
using namespace comskip::media;
#include <algorithm>
#include <limits>

 //AC3

//int bitrate;

//#define PTS_FRAME (double)(1.0 / get_fps())
//#define PTS_FRAME (int) (90000 / get_fps())
//#define SAMPLE_TO_FRAME 2.8125
//#define SAMPLE_TO_FRAME (90000.0/(get_fps() * 1000.0))

//#define BYTERATE	((int)(21400 * 25 / get_fps()))

// The following two functions are undocumented and not included in any public header,
// so we need to declare them ourselves
//extern int  _fseeki64(FILE *, int64_t, int);
//extern int64_t _ftelli64(FILE *);

//test

//extern void set_fps(double frame_delay, double dfps, int ticks, double rfps, double afps);

//extern double fps;

void list_codecs(const comskip::localization::Translator& translator)
{
        const AVCodec *p;
        int * p_i = (int *)NULL;
        int i = 0;
//        avcodec_register_all();
        p = av_codec_iterate((void **)&p_i);
        fputs(translator.text("media_decoders"), stdout);
        printf("---------\n");
        while (p != NULL) {
            if (av_codec_is_decoder(p)) {
                printf("%s", p->name);
                i += strlen(p->name);
                if (i > 80) {
                    printf("\n");
                    i = 0;
                } else
                    printf(", ");
            }
            p = av_codec_iterate((void **)&p_i);
        }
        printf("\n");
}

int SubmitFrame(RecordingContext& context, AVStream        *video_st, AVFrame         *pFrame , double pts)
{
    int res=0;
    int changed = 0;

//	bitrate = pFrame->bit_rate;
    if (pFrame->linesize[0] > max_width || pFrame->height > max_height || pFrame->linesize[0] < 100 || pFrame->height < 100)
    {
        Debug(context, 1, "%s", context.translator.format("media_invalid_frame",
              pFrame->height, pFrame->width, pFrame->linesize[0]).c_str());
        context.state.frame_ptr = NULL;
        return(0);
    }
    if (context.state.height != pFrame->height)
    {
        context.state.height= pFrame->height;
        changed = 1;
    }
    if (context.state.width != pFrame->linesize[0])
    {
        context.state.width= pFrame->linesize[0];
        changed = 1;
    }
    if (context.state.videowidth != pFrame->width)
    {
        context.state.videowidth= pFrame->width;
        changed = 1;
    }
    context.state.ensure_pixel_buffers(comskip::detection::method_enabled(context.settings.commDetectMethod, comskip::detection::DetectionMethod::logo) || context.state.logoInfoAvailable);
    if (changed) {
        if (context.state.initialized) {
            if (comskip::detection::method_enabled(context.settings.commDetectMethod, comskip::detection::DetectionMethod::logo)) InitLogoBuffers(context);
            InitScanLines(context);
            InitHasLogo(context);
        }
        debug_message(context, 5, "media_format_changed", context.state.videowidth, context.state.height);
    }
    context.state.infopos = context.state.headerpos;
    context.state.frame_ptr = pFrame->data[0];
    if (context.state.frame_ptr == NULL)
    {
        return(0);; // return; // comskip::request_exit(2);
    }

    if (pFrame->pict_type == AV_PICTURE_TYPE_B)
        context.state.pict_type = 'B';
    else if (pFrame->pict_type == AV_PICTURE_TYPE_I)
        context.state.pict_type = 'I';
    else
        context.state.pict_type = 'P';

    if (context.state.selftest == 2 && context.state.framenum == 0 && context.state.pass == 0 && context.state.test_pts == 0.0) //Reset file test
        context.state.test_pts = pts;
    if (context.state.selftest == 2 && context.state.pass > 0) //Reset file test
    {
        if (context.state.test_pts != pts)
        {
                comskip::output::write_selftest_log(context.settings.selftest_log_file, "Reset file Failed, initial pts = {:6.3f}, seek pts = {:6.3f}, pass = {}, \"{}\"\n", context.state.test_pts, pts, context.state.pass+1, context.state.video_owner->filename.c_str());
                debug_message(context, 1, "media_selftest_reset_failed", context.state.selftest);
        }
        else
           debug_message(context, 1, "media_selftest_reset_ok");

        comskip::request_exit(1);
    }

    if (!context.state.reviewing)
    {

        print_decode_progress (context, 0);
        res = DetectCommercials(context, (int)context.state.framenum, pts);
        context.state.framenum++;
#ifdef SELFTEST
    if (context.state.selftest == 2 && context.state.pass == 0 && context.state.framenum > 20) //Reset input file
    {
        res = true;
        context.state.pass++;
    }
#endif
        if (res) {
            context.state.framenum = 0;
            context.state.sound_frame_counter = 0;
            context.state.video_owner->seek_req = 1;
            context.state.video_owner->seek_pos = 0;
            context.state.video_owner->seek_pts = 0.0;
        }
    }
    return (res);
}

comskip::media::VideoPacketOutcome video_packet_process(RecordingContext& context, VideoState *is,AVPacket *packet)
{
    double frame_delay;
    int len1, frameFinished = 0;
    int repeat;
    double pts;
//    double dts;
    double real_pts;

//static double prev_frame_delay = 0.0;

    double calculated_delay;

    if (packet && !context.state.reviewing)
    {
        dump_video_start(context);
        dump_video(context,{packet->data,static_cast<std::size_t>(packet->size)});
    }
    real_pts = 0.0;
    pts = 0;
    //is->dec_ctx.get().thread_type
    if (!context.settings.hardware_decode) is->dec_ctx->flags |= AV_CODEC_FLAG_GRAY;
    // Decode video frame
    len1 = avcodec_send_packet(is->dec_ctx.get(), packet);
    comskip::media::require_video_packet_sent(len1);

    // Did we get a video frame?
    while ((len1 = avcodec_receive_frame(is->dec_ctx.get(), is->pFrame.get())) >= 0)
    {
        frameFinished = 1;
        // convert to 8bit
        if (is->pFrame->format == AV_PIX_FMT_YUV420P10LE) {
            if (convert_frame_to_8bit_owned(is->pFrame.get(), is->img_convert_ctx) < 0) {
                Debug(context, 1, "%s", context.translator.text("media_frame_conversion_failed"));
                av_frame_unref(is->pFrame.get());
                continue;
            }
        }

        if(is->dec_ctx->framerate.den && is->dec_ctx->framerate.num)
        {
            frame_delay = (1/ av_q2d(is->dec_ctx->framerate) ) /* * is->ticks_per_frame */ ;
        }
        else
        {
           // Codec time_base is not necessarily a field duration (FFV1, for
           // example). Prefer the demuxer's actual frame rate before applying
           // the legacy MPEG field-time fallback.
           const AVRational rate = av_guess_frame_rate(is->pFormatCtx.get(), is->video_st, is->pFrame.get());
           frame_delay = rate.num > 0 && rate.den > 0
               ? av_q2d(av_inv_q(rate))
               : av_q2d(is->dec_ctx->time_base) * is->ticks_per_frame;
        }

//        frame_delay = av_q2d(is->dec_ctx->time_base) * is->ticks_per_frame ;
        repeat = av_stream_get_parser(is->video_st) ? av_stream_get_parser(is->video_st)->repeat_pict: 4;

 //       if (prev_frame_delay != 0.0 && frame_delay != prev_frame_delay)
 //           Debug(1, "Changing fps from %6.3f to %6.3f", 1.0/prev_frame_delay, 1.0/frame_delay);
        context.state.pev_best_effort_timestamp = context.state.best_effort_timestamp;
        if (context.state.use_cuvid)
            is->pFrame->best_effort_timestamp = is->pFrame->pts;
        context.state.best_effort_timestamp = is->pFrame->best_effort_timestamp;
        calculated_delay = frame_delay;
        if (context.state.best_effort_timestamp != AV_NOPTS_VALUE &&
            context.state.pev_best_effort_timestamp != AV_NOPTS_VALUE)
            calculated_delay = static_cast<double>(static_cast<long double>(context.state.best_effort_timestamp) -
                static_cast<long double>(context.state.pev_best_effort_timestamp)) * av_q2d(is->video_st->time_base);

        if (context.state.best_effort_timestamp == AV_NOPTS_VALUE)
            real_pts = 0;
        else
        {
            context.state.headerpos = comskip::media::input_position(is->pFormatCtx.get(), context.state.headerpos,
                [](AVIOContext* input) { return avio_tell(input); });
            if ((context.state.initial_pts_set < 3 && !context.state.reviewing) || (context.state.reviewing && context.state.initial_pts_set < 2)  )
            {
                if (!approximately_equal(context.state.initial_pts, (context.state.best_effort_timestamp  - (is->video_st->start_time != AV_NOPTS_VALUE ? is->video_st->start_time : 0)) * av_q2d(is->video_st->time_base) - (frame_delay * context.state.framenum) )) {
                    context.state.initial_pts = (context.state.best_effort_timestamp  - (is->video_st->start_time != AV_NOPTS_VALUE ? is->video_st->start_time : 0)) * av_q2d(is->video_st->time_base) - (frame_delay * context.state.framenum);
                    debug_message(context, 10, "media_initial_video_pts",
                                  std::format("{:10.3f}", context.state.initial_pts));
//                    if (timeline_repair<2)
//                        initial_pts = 0.0;
                }
                context.state.initial_pts_set++;
                context.state.final_pts = 0;
                context.state.pts_offset = 0.0;

            }
            real_pts = av_q2d(is->video_st->time_base)* ( context.state.best_effort_timestamp - (is->video_st->start_time != AV_NOPTS_VALUE ? is->video_st->start_time : 0))  - context.state.initial_pts;
            context.state.final_pts = context.state.best_effort_timestamp -  (is->video_st->start_time != AV_NOPTS_VALUE ? is->video_st->start_time : 0);
        }

//        dts =  av_q2d(is->video_st->time_base)* ( is->pFrame->pkt_dts - (is->video_st->start_time != AV_NOPTS_VALUE ? is->video_st->start_time : 0)) ;

        calculated_delay = real_pts - context.state.video_packet_process_prev_real_pts;

        if (context.state.framenum < 500)
        {
            if ( (!(fabs(frame_delay - 0.03336666) < 0.001 )) &&
            ((fabs(calculated_delay - 0.0333333) < 0.0001) || (fabs(calculated_delay - 0.033) < 0.0001) || (fabs(calculated_delay - 0.034) < 0.0001) ||
            (fabs(calculated_delay - 0.067) < 0.0001)     || (fabs(calculated_delay - 0.066) < 0.0001) || (fabs(calculated_delay - 0.06673332) < 0.0001)  ) )
            {
                context.state.video_packet_process_find_29fps++;
            }
            else
                context.state.video_packet_process_find_29fps = 0;
            if (context.state.video_packet_process_force_29fps == 0 && context.state.video_packet_process_find_29fps == 5)
            {
                context.state.video_packet_process_force_29fps = 1;
                context.state.video_packet_process_force_25fps = 0;
                context.state.video_packet_process_force_24fps = 0;
            }

            if ( (!(fabs(frame_delay - 0.040) < 0.001 )) &&
                    (((fabs(calculated_delay - 0.04) < 0.0001)) || (fabs(calculated_delay - 0.039) < 0.0001) || (fabs(calculated_delay - 0.041) < 0.0001)) )
            {
                context.state.video_packet_process_find_25fps++;
            }
            else
                context.state.video_packet_process_find_25fps = 0;
            if (context.state.video_packet_process_force_25fps == 0 && context.state.video_packet_process_find_25fps == 5)
            {
                context.state.video_packet_process_force_29fps = 0;
                context.state.video_packet_process_force_25fps = 1;
                context.state.video_packet_process_force_24fps = 0;
            }

            if ( ((context.state.video_packet_process_find_24fps & 1 ) == 0 && (fabs(calculated_delay - 0.050) < 0.001 )) ||
                    ((context.state.video_packet_process_find_24fps & 1 ) == 1 && (fabs(calculated_delay - 0.033) < 0.001 ))
               )
            {
                context.state.video_packet_process_find_24fps++;
            }
            else
                context.state.video_packet_process_find_24fps = 0;
            if (context.state.video_packet_process_force_24fps == 0 && context.state.video_packet_process_find_24fps == 5)
            {
                context.state.video_packet_process_force_29fps = 0;
                context.state.video_packet_process_force_25fps = 0;
                context.state.video_packet_process_force_24fps = 1;
            }
        }

        if (context.state.video_packet_process_force_29fps && context.state.video_packet_process_find_29fps == 5)
        {
            frame_delay=0.033366666666666669;
            debug_message(context, 1, "media_framerate_forced",
                          std::format("{:6.3f}", 1.0 / frame_delay), context.state.frame_count);
            set_fps(context, frame_delay);
        }
        if (context.state.video_packet_process_force_25fps && context.state.video_packet_process_find_25fps == 5)
        {
            frame_delay=0.04;
            debug_message(context, 1, "media_framerate_forced",
                          std::format("{:6.3f}", 1.0 / frame_delay), context.state.frame_count);
            set_fps(context, frame_delay);
        }
        if (context.state.video_packet_process_force_24fps && context.state.video_packet_process_find_24fps == 5)
        {
            frame_delay=0.0416666666666667;
            debug_message(context, 1, "media_framerate_forced",
                          std::format("{:6.3f}", 1.0 / frame_delay), context.state.frame_count);
            set_fps(context, frame_delay);
        }

//#define SHOW_VIDEO_TIMING
#ifdef SHOW_VIDEO_TIMING
        if (context.state.framenum == 0)
            debug_message(context, 1, "media_video_timing_heading");
        else if (context.state.framenum < 20)
            debug_message(context, 1, "media_video_timing_row",
                          std::format("{:6.5f}", frame_delay / is->ticks_per_frame),
                          is->ticks_per_frame, repeat, std::format("{:6.3f}", real_pts),
                          std::format("{:6.5f}", calculated_delay));
#endif // SHOW_VIDEO_TIMING

        context.state.pts_offset *= 0.9;
        if (!context.state.reviewing && context.settings.timeline_repair) {
            if (context.state.framenum > 1 && fabs(calculated_delay - context.state.pts_offset - frame_delay) < 1.0) { // Allow max 0.5 second timeline jitter to be compensated
                if (!approximately_equal(3*frame_delay/ is->ticks_per_frame, calculated_delay))
                    if (!approximately_equal(1*frame_delay/ is->ticks_per_frame, calculated_delay))
                        context.state.pts_offset = context.state.pts_offset + frame_delay - calculated_delay;
            }
        }
        else
            context.state.do_audio_repair = 0;

//		Debug(0 ,"pst[%3d] = %12.3f, inter = %d, ticks = %d\n", framenum, pts/frame_delay, is->pFrame->interlaced_frame, is->dec_ctxpar->ticks_per_frame);

        // EOF-drained frames may have no timestamp. Residual repair offset is
        // relative to an actual PTS; adding it to zero would reset the clock.
        pts = context.state.best_effort_timestamp == AV_NOPTS_VALUE
            ? is->video_clock : real_pts + context.state.pts_offset;

        calculated_delay = pts - context.state.video_packet_process_prev_pts;

        if (!context.state.reviewing
            && context.state.framenum > 1 && fabs(calculated_delay - frame_delay) > 0.01
            && !approximately_equal(3*frame_delay/ is->ticks_per_frame, calculated_delay)
            && !approximately_equal(2*frame_delay/ is->ticks_per_frame, calculated_delay)
            && !approximately_equal(1*frame_delay/ is->ticks_per_frame, calculated_delay)
            ){
            if ( (context.state.video_packet_process_prev_strange_framenum + 1 != context.state.framenum) &&( context.state.video_packet_process_prev_strange_step < fabs(calculated_delay - frame_delay))) {
                debug_message(context, 8, "media_strange_video_pts_step",
                              std::format("{:6.5f}", calculated_delay + 0.0000005),
                              std::format("{:6.5f}", frame_delay + 0.0000005),
                              context.state.framenum); // Unknown strange step
                if (calculated_delay < -0.5)
                    context.state.do_audio_repair = 0;        // Disable audio repair with messed up video timeline
            }
            context.state.video_packet_process_prev_strange_framenum = context.state.framenum;
            context.state.video_packet_process_prev_strange_step = fabs(calculated_delay - frame_delay);
        }

        // set_fps(calculated_delay, is->fps, repeat, av_q2d(is->video_st->r_frame_rate),  av_q2d(is->video_st->avg_frame_rate));

        if(pts != 0)
        {
            is->video_clock = pts;
            comskip::media::write_timing_row(context, "v   set", real_pts, calculated_delay, pts, is->video_clock, context.state.pts_offset, repeat);
        }
        else
        {
            /* if we aren't given a pts, set it to the clock */
            comskip::media::write_timing_row(context, "v clock", real_pts, calculated_delay, pts, is->video_clock, context.state.pts_offset, repeat);
            pts = is->video_clock;
        }
        is->video_clock_submitted = is->video_clock;

        if (context.state.retries == 0)
        {
            if (is->video_clock - is->seek_pts > -frame_delay / 2.0)
            {

#ifdef SELFTEST
                if (context.state.selftest == 1 && context.state.pass == 1 /*&& framenum > 501 && is->video_clock > 0 */) //Seek test
                {
                   if (is->video_clock < context.state.selftest_target - 0.05 || is->video_clock > context.state.selftest_target + 0.05)
                   {
                    comskip::output::write_selftest_log(context.settings.selftest_log_file, "Seek error: target={:8.1f}, result={:8.1f}, error={:6.3f}, size={:8.1f}, mode={}, \"{}\"\n",
                            is->seek_pts,
                            is->video_clock,
                            is->video_clock - is->seek_pts,
                            is->duration,
                            (is->seek_by_bytes ? "byteseek": "timeseek" ),
                            is->filename.c_str());
                        debug_message(context, 1, "media_selftest_seek_failed");
                   }
                    else
                        debug_message(context, 1, "media_selftest_seek_ok");
                    /*
                                    if (tries ==  0 && fabs((double) av_q2d(is->video_st->time_base)* ((double)(packet->pts - is->video_st->start_time - is->seek_pos ))) > 2.0) {
                                       is->seek_req=1;
                                       is->seek_pos = 20.0 / av_q2d(is->video_st->time_base);
                                       is->seek_flags = AVSEEK_FLAG_BYTE;
                                       tries++;
                                   } else
                     */
                    context.state.selftest = 3;
                    context.settings.live_tv_retries = 1;
                    context.state.pass = 0;
//                    comskip::request_exit(1);
                }
#endif
                if (SubmitFrame (context, is->video_st, is->pFrame.get(), is->video_clock))
                {
                    return comskip::media::VideoPacketOutcome::analysis_complete;
                }
            }
        }
        else {
            if (is->video_clock - is->seek_pts > -frame_delay / 2.0)
            {
                if (context.state.selftest == 3) //Reopen at same location
                {
                    if (is->video_clock < context.state.selftest_target - 0.05 || is->video_clock > context.state.selftest_target + 0.05)
                    {
                        comskip::output::write_selftest_log(context.settings.selftest_log_file, "Reopen error: target={:8.1f}, result={:8.1f}, error={:6.3f}, size={:8.1f}, mode={}, \"{}\"\n",
                            is->seek_pts,
                            is->video_clock,
                            is->video_clock - is->seek_pts,
                            is->duration,
                            (is->seek_by_bytes ? "byteseek": "timeseek" ),
                            is->filename.c_str());
                        debug_message(context, 1, "media_selftest_reopen_failed");
                    }
                    else
                        debug_message(context, 1, "media_selftest_reopen_ok");
                    return comskip::media::VideoPacketOutcome::selftest_complete;
                }
                context.state.retries = 0;
                if (SubmitFrame (context, is->video_st, is->pFrame.get(), is->video_clock))
                {
                    return comskip::media::VideoPacketOutcome::analysis_complete;
                }
            } else {
                if (fabs(is->seek_pts - is->video_clock) > 80 ) {
                    Debug(context, 1, "%s", context.translator.format("media_positioning_failed",
                        std::format("{:6.2f}", is->video_clock)).c_str());
                    if (context.state.selftest == 1 || context.state.selftest == 3)
                    {
                        comskip::output::write_selftest_log(context.settings.selftest_log_file, "Seek error : target={:8.1f}, result={:8.1f}, error={:6.3f}, size={:8.1f}, mode={}, \"{}\"\n",
                            is->seek_pts,
                            is->video_clock,
                            is->video_clock - is->seek_pts,
                            is->duration,
                            (is->seek_by_bytes ? "byteseek": "timeseek" ),
                            is->filename.c_str());
                        debug_message(context, 1, "media_selftest_failed", context.state.selftest);
                    }
                    return context.state.selftest == 1 || context.state.selftest == 3
                        ? comskip::media::VideoPacketOutcome::selftest_complete
                        : comskip::media::VideoPacketOutcome::positioning_failure;
                }
            }
//            if (selftest == 4) comskip::request_exit(1);
        }
        /* update the video clock */
        is->video_clock += frame_delay;
        context.state.video_packet_process_prev_pts = pts;
        context.state.video_packet_process_prev_real_pts = real_pts;
//        prev_frame_delay = frame_delay;

#ifdef PROCESS_CC
        if (is->pFrame->nb_side_data) {
            static_assert(sizeof(context.state.ccData) >= comskip::media::ga94_max_packet_size);
            for (int side_data_index = 0; side_data_index < is->pFrame->nb_side_data; ++side_data_index) {
                const AVFrameSideData *sd = is->pFrame->side_data[side_data_index];
                if (sd->type != AV_FRAME_DATA_A53_CC) continue;
                if (context.captions && !context.state.reviewing)
                    context.captions->consume({sd->data, sd->size}, comskip::media::video_caption_timestamp(pts));
                for (const auto& packet : comskip::media::bridge_a53_captions({sd->data, sd->size})) {
                    std::copy_n(packet.bytes.begin(), packet.size, context.state.ccData);
                    context.state.ccDataLen = static_cast<int>(packet.size);
                    dump_data(context,{context.state.ccData,static_cast<std::size_t>(context.state.ccDataLen)});
                    if (context.state.processCC) ProcessCCData(context);
                }
            }
        }
#endif
    }
    (void)comskip::media::classify_video_receive_status(len1);

    return comskip::media::video_packet_outcome(frameFinished != 0,false,false,false);
}

//extern int dxva2_init(AVCodecContext *s);
