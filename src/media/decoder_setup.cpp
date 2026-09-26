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

#include "decoder.h"
#include "recording_context.h"
#include "app/debug.h"
#include "diagnostic.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <memory>
#include <string_view>
extern "C" {
#include <libavutil/error.h>
}
using namespace comskip::media;

comskip::media::StreamOpenResult stream_component_open(RecordingContext& context, VideoState& is, int stream_index)
{
    AVFormatContext* pFormatCtx = is.pFormatCtx.get();
    AVCodecParameters* codecPar = nullptr;
    AVCodecContext *codecCtx;
    const AVCodec *codec;
    const AVCodec* codec_hw = nullptr;

    if(!pFormatCtx || stream_index < 0 || (unsigned int)stream_index >= pFormatCtx->nb_streams)
    {
        return comskip::media::StreamOpenResult::unavailable;
    }

    if (pFormatCtx->iformat && std::string_view(pFormatCtx->iformat->name) == "mpegts")
        context.state.demux_pid = 1;

    codecPar = pFormatCtx->streams[stream_index]->codecpar;

    codec = avcodec_find_decoder(codecPar->codec_id);

    if (context.state.use_qsv && !codec_hw) {
        if (codecPar->codec_id == AV_CODEC_ID_MJPEG) codec_hw = avcodec_find_decoder_by_name("mjpeg_qsv");
        if (codecPar->codec_id == AV_CODEC_ID_MPEG2VIDEO) codec_hw = avcodec_find_decoder_by_name("mpeg2_qsv");
        if (codecPar->codec_id == AV_CODEC_ID_H264) codec_hw = avcodec_find_decoder_by_name("h264_qsv");
        if (codecPar->codec_id == AV_CODEC_ID_VC1) codec_hw = avcodec_find_decoder_by_name("vc1_qsv");
        if (codecPar->codec_id == AV_CODEC_ID_HEVC) codec_hw = avcodec_find_decoder_by_name("hevc_qsv");
        if (codecPar->codec_id == AV_CODEC_ID_AV1) codec_hw = avcodec_find_decoder_by_name("av1_qsv");
        if (codecPar->codec_id == AV_CODEC_ID_VP8) codec_hw = avcodec_find_decoder_by_name("vp8_qsv");
        if (codecPar->codec_id == AV_CODEC_ID_VP9) codec_hw = avcodec_find_decoder_by_name("vp9_qsv");
    }

    if (context.state.use_dxva2 && !codec_hw) {
        if (codecPar->codec_id == AV_CODEC_ID_MPEG2VIDEO) codec_hw = avcodec_find_decoder_by_name("mpeg2_dxva2");
        if (codecPar->codec_id == AV_CODEC_ID_H264) codec_hw = avcodec_find_decoder_by_name("h264_dxva2");
        if (codecPar->codec_id == AV_CODEC_ID_MPEG4) codec_hw = avcodec_find_decoder_by_name("mpeg4_dxva2");
        if (codecPar->codec_id == AV_CODEC_ID_VC1) codec_hw = avcodec_find_decoder_by_name("vc1_dxva2");
        if (codecPar->codec_id == AV_CODEC_ID_HEVC) codec_hw = avcodec_find_decoder_by_name("hevc_dxva2");
        if (codecPar->codec_id == AV_CODEC_ID_AV1) codec_hw = avcodec_find_decoder_by_name("av1_dxva2");
    }

    if (context.state.use_vdpau && !codec_hw) {
        if (codecPar->codec_id == AV_CODEC_ID_MPEG2VIDEO) codec_hw = avcodec_find_decoder_by_name("mpeg2_vdpau");
        if (codecPar->codec_id == AV_CODEC_ID_H264) codec_hw = avcodec_find_decoder_by_name("h264_vdpau");
        if (codecPar->codec_id == AV_CODEC_ID_MPEG4) codec_hw = avcodec_find_decoder_by_name("mpeg4_vdpau");
        if (codecPar->codec_id == AV_CODEC_ID_VC1) codec_hw = avcodec_find_decoder_by_name("vc1_vdpau");
        if (codecPar->codec_id == AV_CODEC_ID_HEVC) codec_hw = avcodec_find_decoder_by_name("hevc_vdpau");
    }

    if (context.state.use_cuvid && !codec_hw) {
        if (codecPar->codec_id == AV_CODEC_ID_MPEG2VIDEO) codec_hw = avcodec_find_decoder_by_name("mpeg2_cuvid");
        if (codecPar->codec_id == AV_CODEC_ID_H264) codec_hw = avcodec_find_decoder_by_name("h264_cuvid");
        if (codecPar->codec_id == AV_CODEC_ID_HEVC) codec_hw = avcodec_find_decoder_by_name("hevc_cuvid");
        if (codecPar->codec_id == AV_CODEC_ID_MPEG4) codec_hw = avcodec_find_decoder_by_name("mpeg4_cuvid");
        if (codecPar->codec_id == AV_CODEC_ID_VC1) codec_hw = avcodec_find_decoder_by_name("vc1_cuvid");
        if (codecPar->codec_id == AV_CODEC_ID_AV1) codec_hw = avcodec_find_decoder_by_name("av1_cuvid");
    }

    if (context.settings.hardware_decode && !codec_hw) {
        if (codecPar->codec_id == AV_CODEC_ID_MPEG2VIDEO) codec_hw = avcodec_find_decoder_by_name("mpeg2_mmal");
        if (codecPar->codec_id == AV_CODEC_ID_H264) codec_hw = avcodec_find_decoder_by_name("h264_mmal");
        if (codecPar->codec_id == AV_CODEC_ID_MPEG4) codec_hw = avcodec_find_decoder_by_name("mpeg4_mmal");
        if (codecPar->codec_id == AV_CODEC_ID_VC1) codec_hw = avcodec_find_decoder_by_name("vc1_mmal");
    }

    if (codec_hw != nullptr && codec_hw != codec) {
        fputs(context.translator.format("media_using_codec", codec_hw->name, avcodec_get_name(codecPar->codec_id)).c_str(), stderr);
        codec = codec_hw;
    }

    if (!codec) {
        fputs(context.translator.text("media_unsupported_codec"), stderr);
        return comskip::media::StreamOpenResult::unavailable;
    }
    CodecPtr codec_owner(avcodec_alloc_context3(codec));
    codecCtx = codec_owner.get();
    if (!codecCtx) throw std::bad_alloc();
    const int copied = avcodec_parameters_to_context(codecCtx, codecPar);
    if (copied < 0) {
        std::array<char, AV_ERROR_MAX_STRING_SIZE> detail{};
        av_strerror(copied, detail.data(), detail.size());
        throw comskip::diagnostics::DiagnosticError<std::runtime_error>(
            comskip::diagnostics::Code::copying_recording_decoder_parameters_detail, {detail.data()});
    }

    if (codecCtx->codec_type == AVMEDIA_TYPE_VIDEO)
    {
        if (!context.settings.hardware_decode) codecCtx->flags |= AV_CODEC_FLAG_GRAY;

        if (codecCtx->codec_id != AV_CODEC_ID_MPEG1VIDEO) {

#ifdef DONATOR
           codecCtx->thread_count= context.settings.thread_count;
#else
            codecCtx->thread_count= 1;
#endif
        }

        if (codecCtx->codec_id == AV_CODEC_ID_H264) {
            context.state.is_h264 = 1;
#ifdef DONATOR
#else
            Debug(context, 0, context.translator.text("media_public_h264_speed"));
#endif
        }
        else if (context.settings.lowres == 10)
        {
            // Automatic reduction halves the decoded width until it is at most
            // 600 pixels. Both builds accept explicit levels, so the automatic
            // level must not fall through to the codec's maximum reduction.
            int w = codecCtx->width;
            context.settings.lowres = 0;
            while (w > 600) {
                w = w >> 1;
                context.settings.lowres++;
            }
        }

        if (codecCtx->codec_id != AV_CODEC_ID_MPEG1VIDEO) {
#ifdef DONATOR
           codecCtx->thread_count= context.settings.thread_count;
#else
            codecCtx->thread_count= 1;
#endif
        }
    }

    codecCtx->lowres = std::min<int>(codec->max_lowres, context.settings.lowres);
    if (!context.settings.hardware_decode) av_dict_set_int(inout_ptr(context.state.myoptions), "gray", 1, 0);

    if(!codec || (avcodec_open2(codecCtx, codec, inout_ptr(context.state.myoptions)) < 0))
    {
        fputs(context.translator.text("media_unsupported_codec"), stderr);
        return comskip::media::StreamOpenResult::unavailable;
    }

    switch(codecCtx->codec_type)
    {
    case AVMEDIA_TYPE_SUBTITLE:
        is.subtitleStream = stream_index;
        is.subtitle_st = pFormatCtx->streams[stream_index];
        is.subtitle_ctx = std::move(codec_owner);
        if (context.state.demux_pid)
            context.state.selected_subtitle_pid = is.subtitle_st->id;
        break;
    case AVMEDIA_TYPE_AUDIO:
        is.audioStream = stream_index;
        is.audio_st = pFormatCtx->streams[stream_index];
        is.audio_ctx = std::move(codec_owner);

        if (context.state.demux_pid)
            context.state.selected_audio_pid = is.audio_st->id;

        break;
    case AVMEDIA_TYPE_VIDEO:
        is.videoStream = stream_index;
        is.video_st = pFormatCtx->streams[stream_index];
        is.dec_ctx = std::move(codec_owner);

        is.pFrame = make_frame();
        if (!context.settings.hardware_decode) codecCtx->flags |= AV_CODEC_FLAG_GRAY;

        if (codecCtx->codec_id == AV_CODEC_ID_H264)
        {
            context.state.is_h264 = 1;
#ifdef DONATOR
#else
            Debug(context, 0, context.translator.text("media_public_h264_speed"));
#endif
        }

        if (codecCtx->codec_id != AV_CODEC_ID_MPEG1VIDEO) {
#ifdef DONATOR
           codecCtx->thread_count= context.settings.thread_count;
#else
            codecCtx->thread_count= 1;
#endif
        }
        is.ticks_per_frame = (codecCtx->codec_id == AV_CODEC_ID_MPEG1VIDEO) ? 1 : 2;
        if (context.state.demux_pid)
            context.state.selected_video_pid = is.video_st->id;
        
        if (context.settings.skip_B_frames)
            codecCtx->skip_frame = AVDISCARD_NONREF;

        break;
    default:
        break;
    }

    return comskip::media::StreamOpenResult::opened;
}
