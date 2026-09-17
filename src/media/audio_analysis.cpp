// Audio analysis extracted from Comskip's MPEG decoder integration.
// Copyright (C) 2000-2003 Michel Lespinasse and (C) 1999-2000 Aaron Holtzman.
// Distributed under GPL-2.0-or-later; see LICENSE.
#include "audio_analysis.h"
#include "app/debug.h"
#include "recording_context.h"
#include "audio_samples.h"
#include "detection/frame_timestamps.h"
#include "output/media_dump.h"
#include "timing_diagnostics.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <iterator>
#include <limits>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/samplefmt.h>
}

namespace {
constexpr int audio_buffer_capacity = static_cast<int>(std::extent_v<decltype(RecordingState::audio_buffer)>);
constexpr int ac3_buffer_capacity = static_cast<int>(std::extent_v<decltype(RecordingState::ac3_packet)>);
bool same_timestamp(double first, double second) {
    return std::fabs(first - second) < 0.001;
}
template<class... Args>
void audio_debug(RecordingContext& context, int level, std::string_view key, Args&&... args) {
    Debug(context, level, context.translator.format(key, std::forward<Args>(args)...));
}
}

static int retreive_frame_volume(RecordingContext& context, double from_pts, double to_pts)
{
    short *buffer;
    int volume = -1;
    const auto& is = *context.state.video_owner;
    int i;
    double calculated_delay;
    const int sample_rate = is.audio_st->codecpar->sample_rate;
    if (sample_rate <= 0 || !std::isfinite(from_pts) || !std::isfinite(to_pts))
        return -1;
    const double sample_offset = (from_pts - context.state.base_apts) * sample_rate;
    const double sample_count = (to_pts - from_pts) * sample_rate;
    if (sample_offset < -0.5 || sample_offset > context.state.audio_samples
        || sample_count < 0 || sample_count > context.state.audio_samples)
        return -1;
    const int first_sample = static_cast<int>(std::llround(sample_offset));
    const int s_per_frame = static_cast<int>(std::llround(sample_count));


    if (s_per_frame > 1 && first_sample >= 0 && first_sample <= context.state.audio_samples
        && s_per_frame <= context.state.audio_samples - first_sample)
    {
        calculated_delay = 0.0;


 //       Debug(1,"fame=%d, =base=%6.3f, from=%6.3f, samples=%d, to=%6.3f, top==%6.3f\n", -1, base_apts, from_pts, s_per_frame, to_pts, top_apts);
        buffer = &context.state.audio_buffer[first_sample];

        volume = 0;
        for (i = 0; i < s_per_frame; i++)
        {
            volume += (*buffer>0 ? *buffer : - *buffer);
            buffer++;
        }
        volume = volume/s_per_frame;
        comskip::media::write_timing_row(context, "a  read", is.audio_clock, calculated_delay, to_pts, from_pts, volume, s_per_frame);

        const int consumed_samples = first_sample + s_per_frame;
        context.state.audio_samples -= consumed_samples;


        if (volume == 0)
        {
            context.state.frames_without_sound++;
        }
        else if (volume > 20000)
        {
            if (volume > 256000)
                volume = 220000;
            context.state.frames_with_loud_sound++;
            volume = -1;
        }
        else
        {
            context.state.frames_without_sound = 0;
        }

        if (context.state.max_volume_found < volume)
            context.state.max_volume_found = volume;

        // Remove use samples
        context.state.audio_buffer_ptr = context.state.audio_buffer;
        if (context.state.audio_samples > 0)
        {
            for (i = 0; i < context.state.audio_samples; i++)
            {
                *context.state.audio_buffer_ptr++ = *buffer++;
            }
        }
        context.state.base_apts += static_cast<double>(consumed_samples) / sample_rate;
        context.state.top_apts = context.state.base_apts + static_cast<double>(context.state.audio_samples) /
            is.audio_st->codecpar->sample_rate;
        context.state.sound_frame_counter++;
    }
    return(volume);
}

void backfill_frame_volumes(RecordingContext& context)
{
    int f;
    int volume;
    double local_initial_pts = context.state.initial_pts;
    if (context.state.framenum < 3)
        return;
    f = context.state.framenum-2;
    if (std::fabs(local_initial_pts) > 200)
        local_initial_pts = 0;
    while (get_frame_pts(context, f) + local_initial_pts > context.state.base_apts && f > 1) // Find first frame with samples available, could be incomplete
        f--;
    while (f < context.state.framenum-1 && (get_frame_pts(context, f+1) + local_initial_pts )<= context.state.top_apts && (context.state.top_apts - context.state.base_apts) > .2 /* && get_frame_pts(f-1) >= base_apts */) {
        volume = retreive_frame_volume(context, get_frame_pts(context, f) + local_initial_pts, get_frame_pts(context, f+1) + local_initial_pts);
        if (volume > -1) set_frame_volume(context, f, volume);
        f++;
    }
}




void sound_to_frames(RecordingContext& context, VideoState& is, const AVFrame& frame)
{
    const int s = frame.nb_samples;
    const int c = frame.ch_layout.nb_channels;
    if (s <= 0 || c <= 0) return;
    const auto samples = comskip::media::normalize_audio(frame);

    double old_base_apts;

    double calculated_delay = 0.0;
    double avg_volume = 0.0;


    context.state.audio_samples = (context.state.audio_buffer_ptr - context.state.audio_buffer);

    if (context.state.sound_to_frames_old_sample_rate == is.audio_st->codecpar->sample_rate &&
        ((context.state.audio_buffer_ptr - context.state.audio_buffer) < 0 || (context.state.audio_buffer_ptr - context.state.audio_buffer) >= audio_buffer_capacity
        || (context.state.top_apts - context.state.base_apts) * (is.audio_st->codecpar->sample_rate+0.5) > audio_buffer_capacity
        || (context.state.top_apts < context.state.base_apts)
        || !same_timestamp((static_cast<double>(context.state.audio_samples) /
                            (is.audio_st->codecpar->sample_rate + 0.5)) + context.state.base_apts,
                           context.state.top_apts)
        || context.state.audio_samples < 0
        || context.state.audio_samples >= audio_buffer_capacity)) {
       Debug(context, 1, context.translator.text("media_audio_buffer_corrupt"));
       context.state.audio_buffer_ptr = context.state.audio_buffer;
       context.state.top_apts = context.state.base_apts = 0;
       context.state.audio_samples=0;
       return;
    }

    if (context.state.sound_to_frames_old_c != 0 && context.state.sound_to_frames_old_c != c) {
        audio_debug(context, 5, "media_audio_channels_switched",
                    std::format("{:6.5f}", context.state.base_apts),
                    context.state.sound_to_frames_old_c, c);
//        InsertBlackFrame()
    }
    context.state.audio_channels = c;
    context.state.sound_to_frames_old_c = c;
    if (context.state.sound_to_frames_old_sample_rate != 0 && context.state.sound_to_frames_old_sample_rate != is.audio_st->codecpar->sample_rate) {
         audio_debug(context, 5, "media_audio_samplerate_switched",
                     context.state.sound_to_frames_old_sample_rate, is.audio_st->codecpar->sample_rate);
    }
    context.state.sound_to_frames_old_sample_rate = is.audio_st->codecpar->sample_rate;

    old_base_apts = context.state.base_apts;
    // Preserve the sample-derived timeline across sub-millisecond container
    // timestamp rounding. Reanchoring the retained buffer on every packet can
    // make a previously consumed video interval appear available again.
    const double timestamp_precision = std::max(
        av_q2d(is.audio_st->time_base), 1.0 / context.state.sound_to_frames_old_sample_rate);
    if (context.state.audio_samples == 0 || std::fabs(context.state.top_apts - is.audio_clock) > timestamp_precision * 1.1)
        context.state.base_apts = is.audio_clock -
            static_cast<double>(context.state.audio_samples) / is.audio_st->codecpar->sample_rate;
        if (context.settings.ALIGN_AC3_PACKETS && is.audio_st->codecpar->codec_id == AV_CODEC_ID_AC3) {
                    if (   same_timestamp(context.state.base_apts - old_base_apts, 0.032)
                        || same_timestamp(context.state.base_apts - old_base_apts, -0.032)
                        || same_timestamp(context.state.base_apts - old_base_apts, 0.064)
                        || same_timestamp(context.state.base_apts - old_base_apts, -0.064)
                        || same_timestamp(context.state.base_apts - old_base_apts, -0.096)
                        )
                        old_base_apts = context.state.base_apts; // Ignore AC3 packet jitter
            }
    if (old_base_apts != 0.0 && (std::fabs(context.state.base_apts - old_base_apts)>0.01)) {
        audio_debug(context, 8, "media_audio_base_pts_jump",
                    std::format("{:6.5f}", old_base_apts),
                    std::format("{:6.5f}", context.state.base_apts),
                    std::format("{:6.5f}", context.state.base_apts - old_base_apts));
    }

    if (s+context.state.audio_samples > audio_buffer_capacity ) {
        Debug(context, 1, context.translator.text("media_audio_buffer_overflow"));
       context.state.audio_buffer_ptr = context.state.audio_buffer;
       context.state.top_apts = context.state.base_apts = 0;
       context.state.audio_samples=0;
       return;
    }

    // Preserve the existing S16 and floating-point detector scales. Other
    // representations now follow the floating-point path through FFmpeg.
    const double scale = av_get_packed_sample_fmt(static_cast<AVSampleFormat>(frame.format))
        == AV_SAMPLE_FMT_S16 ? 32768.0 : 64000.0;
    for (int i = 0; i < s; ++i) {
        double volume = 0;
        for (const auto& channel : samples.channels) volume += channel[i];
        volume = volume * scale / c;
        // Floating-point streams may exceed the short buffer range or contain
        // nonfinite samples. Avoid undefined conversion and amplitude wraparound.
        if (!std::isfinite(volume)) volume = 0;
        const auto value = static_cast<short>(std::clamp(volume,
            static_cast<double>(std::numeric_limits<short>::lowest()),
            static_cast<double>(std::numeric_limits<short>::max())));
        *context.state.audio_buffer_ptr++ = value;
        avg_volume += std::abs(static_cast<int>(value));
    }
    avg_volume /= s;
    context.state.audio_samples = (context.state.audio_buffer_ptr - context.state.audio_buffer);
    context.state.top_apts = context.state.base_apts + static_cast<double>(context.state.audio_samples) /
        is.audio_st->codecpar->sample_rate;

    calculated_delay = is.audio_clock - context.state.sound_to_frames_old_audio_clock;
    comskip::media::write_timing_row(context, "a frame", is.audio_clock, calculated_delay, context.state.top_apts, context.state.base_apts, avg_volume, s);
    context.state.sound_to_frames_old_audio_clock = is.audio_clock;

    backfill_frame_volumes(context);
}









void audio_packet_process(RecordingContext& context, VideoState& is, AVPacket& pkt)
{
    int prev_codec_id = -1;
    int len1, data_size;
    uint8_t *pp;
    double prev_audio_clock;
//    AC3DecodeContext *s = is.audio_st->codecpar->priv_data;
    int      rps,ps;
    // A local view borrows this input payload; it never owns a buffer reference.
    AVPacket borrowed_audio{};
    AVPacket& pkt_temp = borrowed_audio;

    int got_frame;
    if (!context.state.reviewing)
    {
        dump_audio_start(context);
        dump_audio(context,{pkt.data,static_cast<std::size_t>(pkt.size)});
    }


    pkt_temp.data = pkt.data;
    pkt_temp.size = pkt.size;

    if ( !context.settings.ALIGN_AC3_PACKETS && is.audio_st->codecpar->codec_id == AV_CODEC_ID_AC3
        && (pkt_temp.size < 2 || pkt_temp.data[0] != 0x0b || pkt_temp.data[1] != 0x77))
    {
//        Debug(1, "AC3 packet misaligned, audio decoding will fail\n");
        context.state.ac3_package_misalignment_count++;
    } else {
        context.state.ac3_package_misalignment_count = 0;
    }
    if (!context.settings.ALIGN_AC3_PACKETS && context.state.ac3_package_misalignment_count > 4) {
        audio_debug(context, 8, "media_ac3_packets_misaligned");
        context.settings.ALIGN_AC3_PACKETS = 1;
    }

    if (context.settings.ALIGN_AC3_PACKETS && is.audio_st->codecpar->codec_id == AV_CODEC_ID_AC3) {
        if (pkt_temp.size < 0 || context.state.ac3_packet_index < 0 ||
            context.state.ac3_packet_index > ac3_buffer_capacity ||
            pkt_temp.size > ac3_buffer_capacity - context.state.ac3_packet_index)
        {
            audio_debug(context, 8, "media_ac3_sync_error");
            context.state.ac3_packet_index = 0;
            return;
        }
        std::copy_n(pkt_temp.data, static_cast<std::size_t>(pkt_temp.size),
            context.state.ac3_packet + context.state.ac3_packet_index);
        pkt_temp.data = context.state.ac3_packet;
        pkt_temp.size += context.state.ac3_packet_index;
        context.state.ac3_packet_index = pkt_temp.size;
        ps = 0;
        while (pkt_temp.size >= 2 && (pkt_temp.data[0] != 0x0b || pkt_temp.data[1] != 0x77) ) {
            pkt_temp.data++;
            pkt_temp.size--;
            ps++;
        }
        if (pkt_temp.size < 2) {
            // Keep only the possible first byte of a sync word split across packets.
            const bool partial_sync = pkt_temp.size == 1 && pkt_temp.data[0] == 0x0b;
            context.state.ac3_packet_index = partial_sync ? 1 : 0;
            if (partial_sync)
                context.state.ac3_packet[0] = 0x0b;
            return;
        }
        if (ps>0)
            audio_debug(context, 8, "media_ac3_skipped_bytes", ps, pkt.size, context.state.framenum);
        pp = pkt_temp.data;
        rps = pkt_temp.size-2;
        while (rps > 1 && (pp[rps] != 0x0b || pp[rps+1] != 0x77) ) {
            rps--;
        }
        if (rps >= 2)
        {
            pkt_temp.size = rps;
        }
        else
        {
            // Retain the candidate frame, discarding bytes before its sync word.
            memmove(context.state.ac3_packet, pkt_temp.data, pkt_temp.size);
            context.state.ac3_packet_index = pkt_temp.size;
            return;
        }
        if ( (pkt_temp.size % 768 ) != 0)
            audio_debug(context, 8, "media_ac3_strange_packet_size", rps, context.state.framenum);

    }

    /*  Try to align on packet boundary as some demuxers don't do that, in particular dvr-ms */




    if (pkt.pts != AV_NOPTS_VALUE)
    {
        prev_audio_clock = is.audio_clock;
        is.audio_clock = av_q2d(is.audio_st->time_base)*( pkt.pts -  (is.audio_st->start_time != AV_NOPTS_VALUE ? is.audio_st->start_time : 0)) - context.state.apts_offset;
            if (context.settings.ALIGN_AC3_PACKETS && is.audio_st->codecpar->codec_id == AV_CODEC_ID_AC3) {
                    if (   same_timestamp(is.audio_clock - prev_audio_clock, 0.032)
                        || same_timestamp(is.audio_clock - prev_audio_clock, -0.032)
                        || same_timestamp(is.audio_clock - prev_audio_clock, 0.064)
                        || same_timestamp(is.audio_clock - prev_audio_clock, -0.064)
                        || same_timestamp(is.audio_clock - prev_audio_clock, -0.096)
                        )
                        prev_audio_clock = is.audio_clock; // Ignore AC3 packet jitter
            }

        if ( context.state.initial_apts_set && is.audio_clock != 0.0 && std::fabs( is.audio_clock - prev_audio_clock) > 0.02) {
            if (context.state.do_audio_repair && std::fabs( is.audio_clock - prev_audio_clock) < 1) {
                 is.audio_clock = prev_audio_clock; //Ignore small jitter
            }
            else {
                audio_debug(context, 8, "media_audio_strange_pts_step",
                            std::format("{:6.5f}", (is.audio_clock - prev_audio_clock) + 0.0005),
                            std::format("{:6.5f}", 0.0), context.state.framenum);
                if (context.state.do_audio_repair) {
//                    apts_offset += is.audio_clock - prev_audio_clock ;
//                    is.audio_clock = prev_audio_clock;
                }
            }
        }
        if (!context.state.initial_apts_set) {
            context.state.initial_apts = is.audio_clock;
            audio_debug(context, 10, "media_initial_audio_pts",
                        std::format("{:10.3f}", context.state.initial_apts));

        }
    }

    context.state.initial_apts_set = 1;

    int send_result;
    int received_frames;
    // The realignment buffer may contain a following frame immediately after
    // this packet. FFmpeg requires zero padding at the submitted packet end.
    std::vector<uint8_t> padded_audio;
    AVPacket decoder_packet{};
    decoder_packet.data = pkt_temp.data;
    decoder_packet.size = pkt_temp.size;
    if (context.settings.ALIGN_AC3_PACKETS && is.audio_st->codecpar->codec_id == AV_CODEC_ID_AC3) {
        padded_audio.resize(pkt_temp.size + AV_INPUT_BUFFER_PADDING_SIZE, 0);
        std::copy_n(pkt_temp.data, pkt_temp.size, padded_audio.data());
        decoder_packet.data = padded_audio.data();
    }
    do {
    received_frames = 0;
    send_result = avcodec_send_packet(is.audio_ctx.get(), &decoder_packet);

    // send_packet consumes the whole packet on success. receive_frame returns
    // zero on success, rather than the number of input bytes consumed.
    if (send_result >= 0) {
        pkt_temp.data += pkt_temp.size;
        pkt_temp.size = 0;
    } else if (send_result != AVERROR(EAGAIN)) {
        if (context.settings.ALIGN_AC3_PACKETS && is.audio_st->codecpar->codec_id == AV_CODEC_ID_AC3) {
            const int skipped = pkt_temp.size < 2 ? pkt_temp.size : 2;
            pkt_temp.data += skipped;
            pkt_temp.size -= skipped;
        } else {
            pkt_temp.size = 0;
        }
    }

    //		fprintf(stderr, "sac = %f\n", is.audio_clock);
    while ((len1 = avcodec_receive_frame(is.audio_ctx.get(), is.frame.get())) != AVERROR(EAGAIN))
    {
 //       data_size = STORAGE_SIZE;
        got_frame = len1 >= 0;

        if (prev_codec_id != -1 && static_cast<unsigned int>(prev_codec_id) != is.audio_st->codecpar->codec_id)
        {
            audio_debug(context, 2, "media_audio_format_change");
        }
        prev_codec_id = is.audio_st->codecpar->codec_id;
        if (len1 < 0)
            break;
        ++received_frames;
        if (!got_frame)
        {
            /* stop sending empty packets if the decoder is finished */
            continue;
        }


#if LIBAVCODEC_BUILD >= AV_VERSION_INT(59, 37, 100) && \
    LIBAVUTIL_BUILD >= AV_VERSION_INT(57, 28, 100)
        data_size = av_samples_get_buffer_size(nullptr, is.frame->ch_layout.nb_channels,
                                               is.frame->nb_samples,
                                               static_cast<AVSampleFormat>(is.frame->format), 1);
        if (data_size > 0)
        {
            sound_to_frames(context, is, *is.frame.get());
        }
        is.audio_clock += static_cast<double>(data_size) /
                           (is.frame->ch_layout.nb_channels * is.frame->sample_rate * av_get_bytes_per_sample(static_cast<AVSampleFormat>(is.frame->format)));
        av_frame_unref(is.frame.get());
#else
        data_size = av_samples_get_buffer_size(nullptr, is.frame->channels,
                                               is.frame->nb_samples,
                                               static_cast<AVSampleFormat>(is.frame->format), 1);
        if (data_size > 0)
        {
            sound_to_frames(context, is, *is.frame.get());
        }
        is.audio_clock += static_cast<double>(data_size) /
                           (is.frame->channels * is.frame->sample_rate * av_get_bytes_per_sample(static_cast<AVSampleFormat>(is.frame->format)));
        av_frame_unref(is.frame.get());
#endif
    }

    } while (send_result == AVERROR(EAGAIN) && received_frames > 0);

    // EAGAIN means that no input was accepted. Drain queued frames and retry
    // that same packet; moving on would silently drop non-aligned audio input.
    if (send_result == AVERROR(EAGAIN)) {
        Debug(context, 1, context.translator.text("media_audio_input_refused"));
    }

    if (context.settings.ALIGN_AC3_PACKETS && is.audio_st->codecpar->codec_id == AV_CODEC_ID_AC3) {
        ps = 0;
        rps = (pkt_temp.data - context.state.ac3_packet);
        while (0 < context.state.ac3_packet_index - rps)
        {
            context.state.ac3_packet[ps] = context.state.ac3_packet[rps];
            ps++;
            rps++;
        }
        context.state.ac3_packet_index = ps;
    }
}

