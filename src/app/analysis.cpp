#include "output/selftest_log.h"
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

#include "analysis.h"
#include "debug.h"
#include "recording_context.h"
#include "config/legacy_settings.h"
#include "detection/detector_runtime.h"
#include "ui/review.h"
#include "media/decoder.h"
#include "media/audio_analysis.h"
#include "media/timing_diagnostics.h"
#include "media/ffmpeg_resources.h"
#include "media/stalled_packet_counter.h"
#include "media/video_timestamp.h"
#include "output/media_dump.h"
#include "ui/executable_mode.h"
#include "platform.h"
#include "analysis_policy.h"
#include "comskip.h"
#include "exit_requested.h"
#include <cmath>
#include <filesystem>
#include <format>
#include <utility>

using namespace comskip::media;
#define SELFTEST

namespace {
template<class... Args>
void analysis_debug(RecordingContext& context, int level, std::string_view message_id,
                    Args&&... args)
{
    const auto message = context.translator.format(message_id, std::forward<Args>(args)...);
    Debug(context, level, "%s", message.c_str());
}
}

int comskip_main (RecordingContext& context, int argc, char ** argv)
{
    context.captions.reset();
    struct CaptionOwnerGuard {
        RecordingContext& context;
        ~CaptionOwnerGuard() { context.captions.reset(); }
    } caption_owner_guard{context};
    auto packet_owner = make_packet();
    AVPacket* packet = packet_owner.get();
    int result = 0;
    int ret;
    double tfps;
    double old_clock = 0.0;
    comskip::media::StalledPacketCounter stalled_packets;

    int64_t last_packet_pos = 0;
    int64_t last_packet_pts = 0;
    double retry_target = 0.0;

#ifdef SELFTEST
    //int tries = 0;
#endif
    context.state.retries = 0;


#ifndef _DEBUG
//	__tr y
    {
        //      raise_ exception();
#endif

//		output_debugwindow = 1;

        if (comskip::ui::gui_executable(argv[0]))
            context.settings.output_debugwindow = 1;
        const comskip::platform::ScopedAnalysisPolicy analysis_policy(
            !comskip::ui::gui_executable(argv[0]));
        auto executable_directory = std::filesystem::path(std::u8string_view(
            reinterpret_cast<const char8_t*>(argv[0]))).parent_path();
        if (executable_directory.empty()) executable_directory = ".";
        const auto directory_utf8 = executable_directory.u8string();
        context.state.HomeDir.assign(directory_utf8.begin(), directory_utf8.end());

        context.translator = comskip::localization::Translator::from_arguments(argc, argv);
        fputs(context.translator.format("media_version", PACKAGE_STRING).c_str(), stderr);

#ifndef DONATOR
        fputs(context.translator.text("media_public_build"), stderr);
#else
        fputs(context.translator.text("media_donator_build"), stderr);
#endif

#ifdef _WIN32
#ifdef HAVE_IO_H
//		_setmode (_fileno (stdin), O_BINARY);
//		_setmode (_fileno (stdout), O_BINARY);
#endif
#endif



//
// Wait until recording is complete...
//

//        av_log_set_level(AV_LOG_WARNING);
//        av_log_set_flags(AV_LOG_SKIP_REPEATED);
//
//        av_log_set_level(AV_LOG_WARNING);
        LoadSettings(context, argc, argv, context.translator);

        file_open(context);



        context.state.csRestart = 0;
        context.state.framenum = 0;


        if (context.settings.output_timing)
        {
            comskip::media::open_timing_diagnostics(context);
        }

        av_log_set_level(AV_LOG_INFO);
//        av_log_set_flags(AV_LOG_SKIP_REPEATED);
//

        // main decode loop
again:
        for(;;)
        {
            if(context.state.video_owner->quit)
            {
                break;
            }
            // seek stuff goes here
            if(context.state.video_owner->seek_req)
            {
                if (context.state.video_owner->seek_pts > 0.0)
                {
                    DoSeekRequest(context, context.state.video_owner.get());
                }
                else
                {
                    context.state.video_owner->seek_req = 0;
                    file_close(context);
                    file_open(context);

                    comskip::media::close_timing_diagnostics(context);
                    comskip::media::open_timing_diagnostics(context);
                    close_data(context);
#ifdef PROCESS_CC
                    if (context.captions) context.captions->reset();
#endif
                }
            }
nextpacket:
            ret=av_read_frame(context.state.video_owner->pFormatCtx.get(), packet);

            if (ret>=0 && context.state.video_owner->seek_req)
            {
                double packet_time = (packet->pts - (context.state.video_owner->video_st->start_time != AV_NOPTS_VALUE ? context.state.video_owner->video_st->start_time : 0)) * av_q2d(context.state.video_owner->video_st->time_base);
                if (packet->pts==AV_NOPTS_VALUE || packet->pts == 0 )
                {
                    av_packet_unref(packet);
                    goto nextpacket;
                }
                if (context.state.video_owner->seek_req < 6 && (context.state.video_owner->seek_flags & AVSEEK_FLAG_BYTE) &&  context.state.video_owner->duration > 0 && fabs(packet_time - (context.state.video_owner->seek_pts - 2.5) ) < context.state.video_owner->duration / (10 * context.state.video_owner->seek_req))
                {
                    context.state.video_owner->seek_pos += ((context.state.video_owner->seek_pts - 2.5 - packet_time) / context.state.video_owner->duration ) * avio_size(context.state.video_owner->pFormatCtx->pb) * 0.9;
                    context.state.video_owner->seek_req++;
                    goto again;
                }
                if (context.state.retries)
                    analysis_debug(context, 9, "analysis_retry_packet", last_packet_pos,
                                   packet->pos, last_packet_pts, packet->pts);
                context.state.video_owner->seek_req = 0;
            }
            /*
                    if (ret < 0 && is->seek_req && !is->seek_by_bytes) {
                        is->seek_by_bytes = 1;
                        Set_seek(is, is->seek_pts, is->duration);
                        goto again;
                    }
            */
            context.state.video_owner->seek_req = 0;



#define REOPEN_TIME 500.0

            if ((context.state.selftest == 3 && context.state.retries==0 && context.state.video_owner->video_clock >=REOPEN_TIME))
            {
                ret=AVERROR_EOF;  // Simulate EOF
                context.settings.live_tv = 1;
                context.settings.live_tv_retries = 2;

            }
            if ((context.state.selftest == 4 && context.state.retries==0 && context.state.framenum > 0 && (context.state.framenum % 500) == 0)) // Test reopen every 500 frames
            {
                ret=AVERROR_EOF;  // Simulate EOF
                context.settings.live_tv = 1;
                context.settings.live_tv_retries = 2;

            }
            if(ret < 0 )
            {
                const auto* input = context.state.video_owner->pFormatCtx->pb;
                if (ret == AVERROR_EOF || (input != nullptr && input->eof_reached))
                {
                    if (context.state.selftest == 3)   // Either simulated EOF or real EOF before REOPEN_TIME
                    {
                        if (context.state.retries > 0)
                        {
                            if (context.state.video_owner->video_clock < context.state.selftest_target - 0.05 || context.state.video_owner->video_clock > context.state.selftest_target + 0.05)
                            {
                                comskip::output::write_selftest_log(context.settings.selftest_log_file, "\"{}\": reopen file failed, size={:8.1f}, pts={:6.2f}\n", context.state.video_owner->filename.c_str(), context.state.video_owner->duration, context.state.video_owner->video_clock );
                                analysis_debug(context, 1, "analysis_selftest_failed",
                                               context.state.selftest);
                                comskip::request_exit(1);
                            }
                        }
                        else
                        {
                            if (context.state.video_owner->video_clock < REOPEN_TIME)
                            {
                                context.state.selftest_target = context.state.video_owner->video_clock - 2.0;
                            }
                            else
                            {
                                context.state.selftest_target = REOPEN_TIME;
                            }
                            analysis_debug(context, 1, "analysis_selftest_reopen",
                                           context.state.selftest);
                            context.state.selftest_target = fmax(context.state.selftest_target,0.5);
                            context.settings.live_tv = 1;
                            context.settings.live_tv_retries = 2;

                        }
                    }

                    if ((context.settings.live_tv && context.state.retries < context.settings.live_tv_retries) /* || (selftest == 3 && retries == 0) */)
                    {
                        double frame_delay = av_q2d(context.state.video_owner->dec_ctx->time_base) * context.state.video_owner->ticks_per_frame;
//                    uint64_t retry_target;
                        if (context.state.retries == 0)
                        {
                            if (context.state.selftest == 3)
                                retry_target = context.state.selftest_target;
//                        retry_target = avio_tell(is->pFormatCtx->pb);
                            else
                                retry_target = context.state.video_owner->video_clock + frame_delay;
                        }
                        file_close(context);
                        analysis_debug(context, 1, "analysis_retry",
                                       context.state.retries, context.state.framenum,
                                       std::format("{:8.2f}", retry_target));
                        analysis_debug(context, 9, "analysis_retry_target",
                                       last_packet_pos, last_packet_pts);

                        if (context.state.selftest == 0) sleep_for_ms(4000L);
                        file_open(context);
                        Set_seek(context, context.state.video_owner.get(), retry_target);

                        context.state.retries++;
                        goto again;
                    }

                    // Frame-threaded decoders retain output until an explicit
                    // end-of-input packet. Drain it before finalizing detection.
                    if (context.state.video_owner->dec_ctx.get()) {
                        const auto outcome=video_packet_process(context,context.state.video_owner.get(),NULL);
                        if (outcome==comskip::media::VideoPacketOutcome::selftest_complete) comskip::request_exit(1);
                        if (outcome==comskip::media::VideoPacketOutcome::positioning_failure) comskip::request_exit(-1);
                    }
                    backfill_frame_volumes(context);
                    break;
                }


            }

            if (packet->pts != AV_NOPTS_VALUE && packet->pts != 0 )
            {
                last_packet_pts = packet->pts;
            }
            if (packet->pos != 0 && packet->pos != -1)
            {
                last_packet_pos = packet->pos;
            }

            if(packet->stream_index == context.state.video_owner->videoStream)
            {
                if (packet->size > 0 && packet->data != NULL) {
                    const auto outcome=video_packet_process(context,context.state.video_owner.get(),packet);
                    if (outcome==comskip::media::VideoPacketOutcome::selftest_complete) comskip::request_exit(1);
                    if (outcome==comskip::media::VideoPacketOutcome::positioning_failure) comskip::request_exit(-1);
                }
            }
            else if(packet->stream_index == context.state.video_owner->audioStream)
            {
                if (packet->size > 0 && packet->data != NULL)
                    audio_packet_process(context, context.state.video_owner.get(), packet);
            }
            else if(packet->stream_index == context.state.video_owner->subtitleStream &&
                    context.captions && !context.state.reviewing && packet->size > 0 && packet->data)
            {
                const auto* video = context.state.video_owner->video_st;
                const auto video_origin = (video->start_time != AV_NOPTS_VALUE ?
                    video->start_time * av_q2d(video->time_base) : 0.0) +
                    context.state.initial_pts - context.state.pts_offset;
                context.captions->consume_stream({packet->data, static_cast<std::size_t>(packet->size)},
                    packet->pts, packet->duration,
                    std::chrono::duration_cast<comskip::media::CaptionTimestamp>(std::chrono::duration<double>(video_origin)));
            }
            else
            {

            }
            av_packet_unref(packet);
            const auto video_clock = context.state.video_owner->video_clock;
            if (stalled_packets.observe(video_clock != old_clock))
                Debug(context, 0, "%s", context.translator.text("media_empty_input"));
            old_clock = video_clock;
#ifdef SELFTEST
            if (context.state.selftest == 1 && context.state.pass == 0 && context.state.video_owner->seek_req == 0 && context.state.framenum == 50) //Seek test
            {
                if (context.state.video_owner->duration > 2) {
                    context.state.selftest_target = fmin(450.0, context.state.video_owner->duration - 2);
                } else {
                    context.state.selftest_target = 1.0;
                }
                Set_seek(context, context.state.video_owner.get(), context.state.selftest_target);
                context.state.pass = 1;
                context.state.framenum++;
            }
#endif
        }

        if (context.state.selftest == 1 && context.state.pass == 1 /*&& framenum > 501 && is->video_clock > 0 */)
        {
            if (context.state.video_owner->video_clock < context.state.selftest_target - 0.08 || context.state.video_owner->video_clock > context.state.selftest_target + 0.08)
            {
                comskip::output::write_selftest_log(context.settings.selftest_log_file, "Seek error: target={:8.1f}, result={:8.1f}, error={:6.3f}, size={:8.1f}, mode={}\"{}\"\n",
                        context.state.video_owner->seek_pts,
                        context.state.video_owner->video_clock,
                        context.state.video_owner->video_clock - context.state.video_owner->seek_pts,
                        context.state.video_owner->duration,
                        (context.state.video_owner->seek_by_bytes ? "byteseek": "timeseek" ),
                        context.state.video_owner->filename.c_str());
            } else
                Debug(context, 1, "%s", context.translator.text("analysis_selftest_seek_ok"));

            /*
                            if (tries ==  0 && fabs((double) av_q2d(is->video_st->time_base)* ((double)(packet->pts - is->video_st->start_time - is->seek_pos ))) > 2.0) {
                               is->seek_req=1;
                               is->seek_pos = 20.0 / av_q2d(is->video_st->time_base);
                               is->seek_flags = AVSEEK_FLAG_BYTE;
                               tries++;
                           } else
             */
            context.state.selftest = 3;
            context.state.pass = 0;
            //comskip::request_exit(1);
        }



        if (context.settings.live_tv)
        {
            context.state.lastFrameCommCalculated = 0;
            BuildCommListAsYouGo(context);
        }

    close_data(context);
    if (context.captions) {
            context.captions->finish(comskip::media::video_caption_timestamp(context.state.video_owner->video_clock));
            context.captions.reset();
        }

        tfps = print_decode_progress (context, 1);

        analysis_debug(context, 10, "analysis_parsed_frames", context.state.framenum,
                       context.state.sound_frame_counter, std::format("{:8.2f}", tfps));
        analysis_debug(context, 10, "analysis_maximum_volume", context.state.max_volume_found);


        context.state.in_file.reset();
        if (context.state.framenum>0)
        {
            if(BuildMasterCommList(context))
            {
                result = 1;
                fputs(context.translator.text("media_found_commercials"), stdout);
            }
            else
            {
                result = 0;
                fputs(context.translator.text("media_no_commercials"), stdout);
            }
            if (context.settings.output_debugwindow)
            {
                context.state.processCC = 0;
                fputs(context.translator.text("media_close_window"), stdout);

                comskip::media::close_timing_diagnostics(context);
                if (context.settings.output_timing)
                {
                    context.settings.output_timing = 0;
                }

#if COMSKIP_BUILD_GUI
                while(1)
                {
                    ReviewResult(context);
                    context.window.refresh();
                    sleep_for_ms(100L);
                }
#endif
                //		printf(" Press Enter to close debug window\n");
                //		gets(HomeDir);
            }
        }

#ifndef _DEBUG
    }
//	__exc ept(filter()) /* Stage 3 */
//	{
//      printf("Exception raised, terminating\n");/* Stage 5 of terminating exception */
//		exit(result);
//	}
#endif

#ifdef _WIN32
    return result;
#else
    return !result;
#endif
}
