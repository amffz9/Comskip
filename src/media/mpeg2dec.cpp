#include "recording_context.h"
#include "exit_requested.h"
#include "a53_caption_bridge.h"
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
#include "analysis_policy.h"
#include "comskip.h"
#include "audio_samples.h"
#include "ffmpeg_resources.h"
#include <memory>
using namespace comskip::media;
#include "settings_value.h"
#include "translator.h"
#include <algorithm>
#include <limits>
#include <cmath>
#include <filesystem>
#include "checked_format.h"
#include "frame_conversion.h"

#ifdef HAVE_SDL
#include <SDL.h>
#endif
#include <argtable2.h>
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




#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIOQ_SIZE (5 * 16 * 1024)
#define MAX_VIDEOQ_SIZE (5 * 256 * 1024)
#define AV_SYNC_THRESHOLD 0.01
#define AV_NOSYNC_THRESHOLD 10.0
#define SAMPLE_CORRECTION_PERCENT_MAX 30
#define AUDIO_DIFF_AVG_NB 10
#define FF_ALLOC_EVENT   (SDL_USEREVENT)
#define FF_REFRESH_EVENT (SDL_USEREVENT + 1)
#define FF_QUIT_EVENT (SDL_USEREVENT + 2)
#define VIDEO_PICTURE_QUEUE_SIZE 1
#define DEFAULT_AV_SYNC_TYPE AV_SYNC_ADUIO_MASTER


int convert_frame_to_8bit_owned(AVFrame* frame, ScalerPtr& context) {
    auto* raw_context = context.release();
    const int result = comskip::media::convert_frame_to_8bit(frame, raw_context);
    context.reset(raw_context);
    return result;
}

typedef struct VideoPicture
{
    int width, height; /* source height & width */
    int allocated;
    double pts;
} VideoPicture;








enum
{
    AV_SYNC_AUDIO_MASTER,
    AV_SYNC_VIDEO_MASTER,
    AV_SYNC_EXTERNAL_MASTER,
};


/* Since we only have one decoding thread, the Big Struct
   can be global in case we need it. */










// int width, height;









#define USE_ASF 1


//#include "mpeg2convert.h"
#include "comskip.h"
#include "audio_samples.h"
#include "ffmpeg_resources.h"
#include <memory>
using namespace comskip::media;
#include <algorithm>
#include <limits>




void InitComSkip(RecordingContext& context);
void BuildCommListAsYouGo(RecordingContext& context);
bool ReviewResult(RecordingContext& context);
int video_packet_process(RecordingContext& context, VideoState *is,AVPacket *packet);










 //AC3




#define PIDS	100
#define PID_MASK	0x1fff
























//int bitrate;

//#define PTS_FRAME (double)(1.0 / get_fps())
//#define PTS_FRAME (int) (90000 / get_fps())
//#define SAMPLE_TO_FRAME 2.8125
//#define SAMPLE_TO_FRAME (90000.0/(get_fps() * 1000.0))

//#define BYTERATE	((int)(21400 * 25 / get_fps()))

#define   FSEEK    _fseeki64
#define   FTELL    _ftelli64
// The following two functions are undocumented and not included in any public header,
// so we need to declare them ourselves
//extern int  _fseeki64(FILE *, int64_t, int);
//extern int64_t _ftelli64(FILE *);







//test

#define DUMP_OPEN if (context.settings.output_timing) { sprintf(context.state.tempstring, "%s.timing.csv", context.state.inbasename); context.state.timing_file.reset(myfopen(context.state.tempstring, "w")); DUMP_HEADER }
#define DUMP_HEADER if (context.state.timing_file.get()) fprintf(context.state.timing_file.get(), "sep=,\ntype   ,real_pts, step        ,pts         ,clock       ,delta       ,offset, repeat\n");
#define DUMP_TIMING(T, D, P, C, O, S) if (context.state.timing_file.get() && !context.state.csStepping && !context.state.csJumping && !context.state.csStartJump) fprintf(context.state.timing_file.get(), "%7s, %12.3f, %12.3f, %12.3f, %12.3f, %12.3f, %12.3f, %d\n", \
    T, (double) (D), (double) calculated_delay, (double) (P), (double) (C), ((double) (P) - (double) (C)), (O), (S));
#define DUMP_CLOSE context.state.timing_file.reset();











//extern void set_fps(double frame_delay, double dfps, int ticks, double rfps, double afps);
extern void set_fps(RecordingContext& context, double frame_delay);
extern void dump_video (RecordingContext& context, char *start, char *end);
extern void dump_audio (RecordingContext& context, char *start, char *end);
extern void	Debug(RecordingContext& context, int level, const char * fmt, ...);
extern void dump_video_start(RecordingContext& context);
extern void dump_audio_start(RecordingContext& context);
void file_open(RecordingContext& context);
int DetectCommercials(RecordingContext& context, int, double);
bool BuildMasterCommList(RecordingContext& context);
FILE* LoadSettings(RecordingContext& context, int argc, char ** argv, const comskip::localization::Translator& translator);
void ProcessCCData(RecordingContext& context);
void dump_data(RecordingContext& context, char *start, int length);
void close_data(RecordingContext& context);



#define AUDIOBUFFER	1600000





#define ISSAME(T1,T2) (fabs((T1) - (T2)) < 0.001)

//extern double fps;

extern double get_fps(RecordingContext& context);
extern int get_samplerate();
extern int get_channels();
extern void add_volumes(int *volumes, int nr_frames);
extern void set_frame_volume(RecordingContext& context, uint32_t framenr, int volume);

extern double get_frame_pts(RecordingContext& context, int f);






#define MAX_FRAMES_WITHOUT_SOUND	100



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


int retreive_frame_volume(RecordingContext& context, double from_pts, double to_pts)
{
    short *buffer;
    int volume = -1;
    VideoState *is = context.state.video_owner.get();
    int i;
    double calculated_delay;
    const int sample_rate = is->audio_st->codecpar->sample_rate;
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
        if (context.state.sample_file.get()) fprintf(context.state.sample_file.get(), "Frame %i\n", context.state.sound_frame_counter);
        for (i = 0; i < s_per_frame; i++)
        {
            if (context.state.sample_file.get()) fprintf(context.state.sample_file.get(), "%i\n", *buffer);
            volume += (*buffer>0 ? *buffer : - *buffer);
            buffer++;
        }
        volume = volume/s_per_frame;
        DUMP_TIMING("a  read", is->audio_clock, to_pts, from_pts, (double)volume, s_per_frame);

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
        context.state.top_apts = context.state.base_apts + context.state.audio_samples / (double)(is->audio_st->codecpar->sample_rate);
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
    if (fabs(local_initial_pts) > 200)
        local_initial_pts = 0;
    while (get_frame_pts(context, f) + local_initial_pts > context.state.base_apts && f > 1) // Find first frame with samples available, could be incomplete
        f--;
    while (f < context.state.framenum-1 && (get_frame_pts(context, f+1) + local_initial_pts )<= context.state.top_apts && (context.state.top_apts - context.state.base_apts) > .2 /* && get_frame_pts(f-1) >= base_apts */) {
        volume = retreive_frame_volume(context, get_frame_pts(context, f) + local_initial_pts, get_frame_pts(context, f+1) + local_initial_pts);
        if (volume > -1) set_frame_volume(context, f, volume);
        f++;
    }
}




void sound_to_frames(RecordingContext& context, VideoState *is, const AVFrame& frame)
{
    const int s = frame.nb_samples;
    const int c = frame.ch_layout.nb_channels;
    if (s <= 0 || c <= 0) return;
    const auto samples = comskip::media::normalize_audio(frame);

    double old_base_apts;

    double calculated_delay = 0.0;
    double avg_volume = 0.0;


    context.state.audio_samples = (context.state.audio_buffer_ptr - context.state.audio_buffer);

    if (context.state.sound_to_frames_old_sample_rate == is->audio_st->codecpar->sample_rate &&
        ((context.state.audio_buffer_ptr - context.state.audio_buffer) < 0 || (context.state.audio_buffer_ptr - context.state.audio_buffer) >= AUDIOBUFFER
        || (context.state.top_apts - context.state.base_apts) * (is->audio_st->codecpar->sample_rate+0.5) > AUDIOBUFFER
        || (context.state.top_apts < context.state.base_apts)
        || !ISSAME(((double)context.state.audio_samples /(double)(is->audio_st->codecpar->sample_rate+0.5))+ context.state.base_apts, context.state.top_apts)
        || context.state.audio_samples < 0
        || context.state.audio_samples >= AUDIOBUFFER)) {
       Debug(context, 1, "Panic: Audio buffering corrupt\n");
       context.state.audio_buffer_ptr = context.state.audio_buffer;
       context.state.top_apts = context.state.base_apts = 0;
       context.state.audio_samples=0;
       return;
    }

    if (context.state.sound_to_frames_old_c != 0 && context.state.sound_to_frames_old_c != c) {
        Debug(context, 5, "Audio channels switched at pts=%6.5f from %d to %d\n", context.state.base_apts, context.state.sound_to_frames_old_c, c);
//        InsertBlackFrame()
    }
    context.state.audio_channels = c;
    context.state.sound_to_frames_old_c = c;
    if (context.state.sound_to_frames_old_sample_rate != 0 && context.state.sound_to_frames_old_sample_rate != is->audio_st->codecpar->sample_rate) {
         Debug(context, 5, "Audio samplerate switched from %d to %d\n", context.state.sound_to_frames_old_sample_rate, is->audio_st->codecpar->sample_rate );
    }
    context.state.sound_to_frames_old_sample_rate = is->audio_st->codecpar->sample_rate;

    old_base_apts = context.state.base_apts;
    // Preserve the sample-derived timeline across sub-millisecond container
    // timestamp rounding. Reanchoring the retained buffer on every packet can
    // make a previously consumed video interval appear available again.
    const double timestamp_precision = std::max(
        av_q2d(is->audio_st->time_base), 1.0 / context.state.sound_to_frames_old_sample_rate);
    if (context.state.audio_samples == 0 || fabs(context.state.top_apts - is->audio_clock) > timestamp_precision * 1.1)
        context.state.base_apts = (is->audio_clock - ((double)context.state.audio_samples /(double)(is->audio_st->codecpar->sample_rate)));
        if (context.settings.ALIGN_AC3_PACKETS && is->audio_st->codecpar->codec_id == AV_CODEC_ID_AC3) {
                    if (   ISSAME(context.state.base_apts - old_base_apts, 0.032)
                        || ISSAME(context.state.base_apts - old_base_apts, -0.032)
                        || ISSAME(context.state.base_apts - old_base_apts, 0.064)
                        || ISSAME(context.state.base_apts - old_base_apts, -0.064)
                        || ISSAME(context.state.base_apts - old_base_apts, -0.096)
                        )
                        old_base_apts = context.state.base_apts; // Ignore AC3 packet jitter
            }
    if (old_base_apts != 0.0 && (fabs(context.state.base_apts - old_base_apts)>0.01)) {
        Debug(context, 8, "Jump in base apts from %6.5f to %6.5f, delta=%6.5f\n",old_base_apts, context.state.base_apts, context.state.base_apts -old_base_apts);
    }

    if (s+context.state.audio_samples > AUDIOBUFFER ) {
        Debug(context, 1,"Panic: Audio buffer overflow, resetting audio buffer\n");
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
    context.state.top_apts = context.state.base_apts + context.state.audio_samples / (double)(is->audio_st->codecpar->sample_rate);

    calculated_delay = is->audio_clock - context.state.sound_to_frames_old_audio_clock;
    DUMP_TIMING("a frame", is->audio_clock, context.state.top_apts, context.state.base_apts, avg_volume,s);
    context.state.sound_to_frames_old_audio_clock = is->audio_clock;

    backfill_frame_volumes(context);
}


#define AC3_BUFFER_SIZE 100000






void audio_packet_process(RecordingContext& context, VideoState *is, AVPacket *pkt)
{
    int prev_codec_id = -1;
    int len1, data_size;
    uint8_t *pp;
    double prev_audio_clock;
//    AC3DecodeContext *s = is->audio_st->codecpar->priv_data;
    int      rps,ps;
    AVPacket *pkt_temp = &is->audio_pkt_temp;

    int got_frame;
    if (!context.state.reviewing)
    {
        dump_audio_start(context);
        dump_audio(context, (char *)pkt->data,(char *) (pkt->data + pkt->size));
    }


    pkt_temp->data = pkt->data;
    pkt_temp->size = pkt->size;

    if ( !context.settings.ALIGN_AC3_PACKETS && is->audio_st->codecpar->codec_id == AV_CODEC_ID_AC3
        && (pkt_temp->size < 2 || pkt_temp->data[0] != 0x0b || pkt_temp->data[1] != 0x77))
    {
//        Debug(1, "AC3 packet misaligned, audio decoding will fail\n");
        context.state.ac3_package_misalignment_count++;
    } else {
        context.state.ac3_package_misalignment_count = 0;
    }
    if (!context.settings.ALIGN_AC3_PACKETS && context.state.ac3_package_misalignment_count > 4) {
        Debug(context, 8, "AC3 packets misaligned, enabling AC3 re-alignment\n");
        context.settings.ALIGN_AC3_PACKETS = 1;
    }

    if (context.settings.ALIGN_AC3_PACKETS && is->audio_st->codecpar->codec_id == AV_CODEC_ID_AC3) {
        if (pkt_temp->size < 0 || pkt_temp->size > AC3_BUFFER_SIZE - context.state.ac3_packet_index)
        {
            Debug(context, 8,"AC3 sync error\n");
            context.state.ac3_packet_index = 0;
            return;
        }
        memcpy(&context.state.ac3_packet[context.state.ac3_packet_index], pkt_temp->data, pkt_temp->size);
        pkt_temp->data = context.state.ac3_packet;
        pkt_temp->size += context.state.ac3_packet_index;
        context.state.ac3_packet_index = pkt_temp->size;
        ps = 0;
        while (pkt_temp->size >= 2 && (pkt_temp->data[0] != 0x0b || pkt_temp->data[1] != 0x77) ) {
            pkt_temp->data++;
            pkt_temp->size--;
            ps++;
        }
        if (pkt_temp->size < 2) {
            // Keep only the possible first byte of a sync word split across packets.
            const bool partial_sync = pkt_temp->size == 1 && pkt_temp->data[0] == 0x0b;
            context.state.ac3_packet_index = partial_sync ? 1 : 0;
            if (partial_sync)
                context.state.ac3_packet[0] = 0x0b;
            return;
        }
        if (ps>0)
            Debug(context, 8,"Skipped %d of added %d bytes in audio input stream around frame %d\n", ps, pkt->size, context.state.framenum);
        pp = pkt_temp->data;
        rps = pkt_temp->size-2;
        while (rps > 1 && (pp[rps] != 0x0b || pp[rps+1] != 0x77) ) {
            rps--;
        }
        if (rps >= 2)
        {
            pkt_temp->size = rps;
        }
        else
        {
            // Retain the candidate frame, discarding bytes before its sync word.
            memmove(context.state.ac3_packet, pkt_temp->data, pkt_temp->size);
            context.state.ac3_packet_index = pkt_temp->size;
            return;
        }
        if ( (pkt_temp->size % 768 ) != 0)
            Debug(context, 8,"Strange packet size of %d bytes in audio input stream around frame %d\n", rps, context.state.framenum);

    }

    /*  Try to align on packet boundary as some demuxers don't do that, in particular dvr-ms */




    if (pkt->pts != AV_NOPTS_VALUE)
    {
        prev_audio_clock = is->audio_clock;
        is->audio_clock = av_q2d(is->audio_st->time_base)*( pkt->pts -  (is->audio_st->start_time != AV_NOPTS_VALUE ? is->audio_st->start_time : 0)) - context.state.apts_offset;
            if (context.settings.ALIGN_AC3_PACKETS && is->audio_st->codecpar->codec_id == AV_CODEC_ID_AC3) {
                    if (   ISSAME(is->audio_clock - prev_audio_clock, 0.032)
                        || ISSAME(is->audio_clock - prev_audio_clock, -0.032)
                        || ISSAME(is->audio_clock - prev_audio_clock, 0.064)
                        || ISSAME(is->audio_clock - prev_audio_clock, -0.064)
                        || ISSAME(is->audio_clock - prev_audio_clock, -0.096)
                        )
                        prev_audio_clock = is->audio_clock; // Ignore AC3 packet jitter
            }

        if ( context.state.initial_apts_set && is->audio_clock != 0.0 && fabs( is->audio_clock - prev_audio_clock) > 0.02) {
            if (context.state.do_audio_repair && fabs( is->audio_clock - prev_audio_clock) < 1) {
                 is->audio_clock = prev_audio_clock; //Ignore small jitter
            }
            else {
                Debug(context, 8 ,"Strange audio pts step of %6.5f instead of %6.5f at frame %d\n", (is->audio_clock - prev_audio_clock)+0.0005, 0.0 , context.state.framenum);
                if (context.state.do_audio_repair) {
//                    apts_offset += is->audio_clock - prev_audio_clock ;
//                    is->audio_clock = prev_audio_clock;
                }
            }
        }
        if (!context.state.initial_apts_set) {
            context.state.initial_apts = is->audio_clock;
            Debug(context,  10,"\nInitial audio pts = %10.3f\n", context.state.initial_apts);

        }
    }

    context.state.initial_apts_set = 1;

    int send_result;
    int received_frames;
    // The realignment buffer may contain a following frame immediately after
    // this packet. FFmpeg requires zero padding at the submitted packet end.
    std::vector<uint8_t> padded_audio;
    AVPacket decoder_packet{};
    decoder_packet.data = pkt_temp->data;
    decoder_packet.size = pkt_temp->size;
    if (context.settings.ALIGN_AC3_PACKETS && is->audio_st->codecpar->codec_id == AV_CODEC_ID_AC3) {
        padded_audio.resize(pkt_temp->size + AV_INPUT_BUFFER_PADDING_SIZE, 0);
        std::copy_n(pkt_temp->data, pkt_temp->size, padded_audio.data());
        decoder_packet.data = padded_audio.data();
    }
retry_audio_send:
    received_frames = 0;
    send_result = avcodec_send_packet(is->audio_ctx.get(), &decoder_packet);

    // send_packet consumes the whole packet on success. receive_frame returns
    // zero on success, rather than the number of input bytes consumed.
    if (send_result >= 0) {
        pkt_temp->data += pkt_temp->size;
        pkt_temp->size = 0;
    } else if (send_result != AVERROR(EAGAIN)) {
        if (context.settings.ALIGN_AC3_PACKETS && is->audio_st->codecpar->codec_id == AV_CODEC_ID_AC3) {
            const int skipped = pkt_temp->size < 2 ? pkt_temp->size : 2;
            pkt_temp->data += skipped;
            pkt_temp->size -= skipped;
        } else {
            pkt_temp->size = 0;
        }
    }

    //		fprintf(stderr, "sac = %f\n", is->audio_clock);
    while ((len1 = avcodec_receive_frame(is->audio_ctx.get(), is->frame.get())) != AVERROR(EAGAIN))
    {
 //       data_size = STORAGE_SIZE;
        got_frame = len1 >= 0;

        if (prev_codec_id != -1 && (unsigned int)prev_codec_id != is->audio_st->codecpar->codec_id)
        {
            Debug(context, 2 ,"Audio format change\n");
        }
        prev_codec_id = is->audio_st->codecpar->codec_id;
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
        data_size = av_samples_get_buffer_size(NULL, is->frame->ch_layout.nb_channels,
                                               is->frame->nb_samples,
                                               static_cast<AVSampleFormat>(is->frame->format), 1);
        if (data_size > 0)
        {
            sound_to_frames(context, is, *is->frame.get());
        }
        is->audio_clock += (double)data_size /
                           (is->frame->ch_layout.nb_channels * is->frame->sample_rate * av_get_bytes_per_sample(static_cast<AVSampleFormat>(is->frame->format)));
        av_frame_unref(is->frame.get());
#else
        data_size = av_samples_get_buffer_size(NULL, is->frame->channels,
                                               is->frame->nb_samples,
                                               static_cast<AVSampleFormat>(is->frame->format), 1);
        if (data_size > 0)
        {
            sound_to_frames(is, *is->frame.get());
        }
        is->audio_clock += (double)data_size /
                           (is->frame->channels * is->frame->sample_rate * av_get_bytes_per_sample(static_cast<AVSampleFormat>(is->frame->format)));
        av_frame_unref(is->frame.get());
#endif
    }

    // EAGAIN means that no input was accepted. Drain queued frames and retry
    // that same packet; moving on would silently drop non-aligned audio input.
    if (send_result == AVERROR(EAGAIN)) {
        if (received_frames > 0)
            goto retry_audio_send;
        Debug(context, 1, "Audio decoder refused input without producing a frame\n");
    }

    if (context.settings.ALIGN_AC3_PACKETS && is->audio_st->codecpar->codec_id == AV_CODEC_ID_AC3) {
        ps = 0;
        rps = (pkt_temp->data - context.state.ac3_packet);
        while (0 < context.state.ac3_packet_index - rps)
        {
            context.state.ac3_packet[ps] = context.state.ac3_packet[rps];
            ps++;
            rps++;
        }
        context.state.ac3_packet_index = ps;
    }
}

static double print_fps (RecordingContext& context, int final)
{




    struct timeval tv_end;
    double fps, tfps;
    int frames, elapsed;
    char cur_pos[100] = "0:00:00";

    if (context.state.decoder_verbose)
        return 0.0;

    if(context.state.csStepping)
        return 0.0;

    if(final < 0)
    {
        context.state.print_fps_frame_counter = 0;
        context.state.print_fps_last_count = 0;
        return 0.0;
    }
#ifdef DONATOR
#else
#ifndef DEBUG
again:
#endif
#endif
    gettimeofday (&tv_end, NULL);

    if (!context.state.print_fps_frame_counter)
    {
        context.state.print_fps_tv_start = context.state.print_fps_tv_beg = tv_end;
    }

    elapsed = (tv_end.tv_sec - context.state.print_fps_tv_beg.tv_sec) * 100 + (tv_end.tv_usec - context.state.print_fps_tv_beg.tv_usec) / 10000;
    context.state.print_fps_total_elapsed = (tv_end.tv_sec - context.state.print_fps_tv_start.tv_sec) * 100 + (tv_end.tv_usec - context.state.print_fps_tv_start.tv_usec) / 10000;

    if (final)
    {
        if (context.state.print_fps_total_elapsed)
            tfps = context.state.print_fps_frame_counter * 100.0 / context.state.print_fps_total_elapsed;
        else
            tfps = 0;

        fputs(context.translator.format("media_decoded_summary", context.state.print_fps_frame_counter,
              std::format("{:.2f}", context.state.print_fps_total_elapsed / 100.0),
              std::format("{:.2f}", tfps)).c_str(), stderr);
        fflush(stderr);
        return tfps;
    }

    context.state.print_fps_frame_counter++;

    frames = context.state.print_fps_frame_counter - context.state.print_fps_last_count;

#ifdef DONATOR
#else
#ifndef DEBUG
    if (is_h264 && frames > 15 &&  elapsed < 100)
    {
        sleep_for_ms(100L);
        goto again;
    }
#endif
#endif

    if (elapsed < 100)	/* only display every 1.00 seconds */
        return 0.0;

    context.state.print_fps_tv_beg = tv_end;

//    cur_second = (int)(get_frame_pts(framenum));
    context.state.cur_second = (int)((context.state.framenum)/get_fps(context));
    context.state.cur_hour = context.state.cur_second / (60 * 60);
    context.state.cur_second -= context.state.cur_hour * 60 * 60;
    context.state.cur_minute = context.state.cur_second / 60;
    context.state.cur_second -= context.state.cur_minute * 60;


    sprintf(cur_pos, "%2i:%.2i:%.2i", context.state.cur_hour, context.state.cur_minute, context.state.cur_second);

    fps = frames * 100.0 / elapsed;
    tfps = context.state.print_fps_frame_counter * 100.0 / context.state.print_fps_total_elapsed;

    fputs(context.translator.format("media_decode_progress", cur_pos, context.state.print_fps_frame_counter,
          std::format("{:.2f}", context.state.print_fps_total_elapsed / 100.0), std::format("{:.2f}", tfps),
          std::format("{:.2f}", elapsed / 100.0), std::format("{:.2f}", fps),
          static_cast<int>(100.0 * context.state.framenum / get_fps(context) /
                           context.state.video_owner->duration)).c_str(), stderr);
    fputc('\r', stderr);
    fflush(stderr);
    context.state.print_fps_last_count = context.state.print_fps_frame_counter;
    return tfps;
}

#ifdef PROCESS_CC
#endif


int SubmitFrame(RecordingContext& context, AVStream        *video_st, AVFrame         *pFrame , double pts)
{
    int res=0;
    int changed = 0;

//	bitrate = pFrame->bit_rate;
    if (pFrame->linesize[0] > MAXWIDTH || pFrame->height > MAXHEIGHT || pFrame->linesize[0] < 100 || pFrame->height < 100)
    {
        Debug(context, 1, "Panic: illegal height (%d), width (%d) or frame period (%d)\n",
              pFrame->height, pFrame->width, pFrame->linesize[0]);
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
    context.state.ensure_pixel_buffers((context.settings.commDetectMethod & LOGO) != 0 || context.state.logoInfoAvailable);
    if (changed) {
        if (context.state.initialized) {
            if (context.settings.commDetectMethod & LOGO) InitLogoBuffers(context);
            InitScanLines(context);
            InitHasLogo(context);
        }
        Debug(context, 5, "Format changed to [%d : %d]\n", context.state.videowidth, context.state.height);
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
               context.state.sample_file.reset(fopen("seektest.log", "a+"));
                fprintf(context.state.sample_file.get(), "Reset file Failed, initial pts = %6.3f, seek pts = %6.3f, pass = %d, \"%s\"\n", context.state.test_pts, pts, context.state.pass+1, context.state.video_owner->filename);
                context.state.sample_file.reset();
                Debug(context,  1,"\nSelftest %d FAILED: Reset\n", context.state.selftest);
        }
        else
           Debug(context,  1,"\nSelftest 2 OK: Reset\n");

        comskip::request_exit(1);
    }

    if (!context.state.reviewing)
    {

        print_fps (context, 0);
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

    if (is->seek_by_bytes)
    {
//                            pos = avio_tell(is->pFormatCtx->pb);
      uint64_t size =  avio_size(ic->pb);
        if (length < 0) {
            is->seek_pos = size*fmax(0,pts-4.0)/(context.state.frame_count * get_fps(context));
//            Debug(0,"Impossible to reposition this file, aborting\n");
  //          comskip::request_exit(-1);
        } else {
            is->seek_pos = size*fmax(0,pts-4.0)/length;
        }
        is->seek_flags |= AVSEEK_FLAG_BYTE;
    } else {
        pts = fmax(0,pts+context.state.initial_pts);
        is->seek_pos = pts / av_q2d(is->video_st->time_base);
        if (is->video_st->start_time != AV_NOPTS_VALUE)
        {
            is->seek_pos += is->video_st->start_time;
        }
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
            context.state.sample_file.reset(fopen("seektest.log", "a+"));
            fprintf(context.state.sample_file.get(), "%s error while seeking, target=%6.3f, \"%s\"\n", error_text,is->seek_pts, is->pFormatCtx->url);
            context.state.sample_file.reset();
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
    AVPacket *packet;
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
    packet = &(is->audio_pkt);

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




namespace {
comskip::media::CaptionTimestamp caption_timestamp(double seconds) {
    if (!std::isfinite(seconds) || seconds >= static_cast<double>(std::numeric_limits<std::int64_t>::max()) / 1000000)
        throw std::invalid_argument("Invalid video caption timestamp");
    seconds = std::max(0.0, seconds);
    return std::chrono::duration_cast<comskip::media::CaptionTimestamp>(std::chrono::duration<double>(seconds));
}
}

int video_packet_process(RecordingContext& context, VideoState *is,AVPacket *packet)
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
        dump_video(context, (char *)packet->data,(char *) (packet->data + packet->size));
    }
    real_pts = 0.0;
    pts = 0;
    //is->dec_ctx.get().thread_type
    if (!context.settings.hardware_decode) is->dec_ctx->flags |= AV_CODEC_FLAG_GRAY;
    // Decode video frame
    len1 = avcodec_send_packet(is->dec_ctx.get(), packet);

    if (len1<0)
    {
/*
        if (len1 == -1 && thread_count > 1)
        {
            InitComSkip();
            thread_count = 1;
            is->seek_req = 1;
            is->seek_pos = 0;
            is->seek_pts = 0.0;
            pev_best_effort_timestamp = 0;
            best_effort_timestamp = 0;
            framenum = 1;
            Debug(1 ,"Restarting processing in single thread mode because frame size is changing \n");
            goto quit;
        }
  */
    }

    // Did we get a video frame?
    while ((len1 = avcodec_receive_frame(is->dec_ctx.get(), is->pFrame.get())) >= 0)
    {
        frameFinished = 1;
        // convert to 8bit
        if (is->pFrame->format == AV_PIX_FMT_YUV420P10LE) {
            if (convert_frame_to_8bit_owned(is->pFrame.get(), is->img_convert_ctx) < 0) {
                Debug(context, 1, "Could not convert the decoded 10-bit frame to 8-bit\n");
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
            context.state.headerpos = avio_tell(is->pFormatCtx->pb);
            if ((context.state.initial_pts_set < 3 && !context.state.reviewing) || (context.state.reviewing && context.state.initial_pts_set < 2)  )
            {
//              if (!ISSAME(initial_pts, av_q2d(is->video_st->time_base)* (best_effort_timestamp - (frame_delay * framenum) / av_q2d(is->video_st->time_base) - (is->video_st->start_time != AV_NOPTS_VALUE ? is->video_st->start_time : 0)))) {
                if (!ISSAME(context.state.initial_pts, (context.state.best_effort_timestamp  - (is->video_st->start_time != AV_NOPTS_VALUE ? is->video_st->start_time : 0)) * av_q2d(is->video_st->time_base) - (frame_delay * context.state.framenum) )) {
                    context.state.initial_pts = (context.state.best_effort_timestamp  - (is->video_st->start_time != AV_NOPTS_VALUE ? is->video_st->start_time : 0)) * av_q2d(is->video_st->time_base) - (frame_delay * context.state.framenum);
                    Debug(context,  10,"\nInitial video pts = %10.3f\n", context.state.initial_pts);
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
            Debug(context, 1 ,"Framerate forced %6.3f fps at frame %d\n", 1.0/frame_delay, context.state.frame_count);
            set_fps(context, frame_delay);
        }
        if (context.state.video_packet_process_force_25fps && context.state.video_packet_process_find_25fps == 5)
        {
            frame_delay=0.04;
            Debug(context, 1 ,"Framerate forced %6.3f fps at frame %d\n", 1.0/frame_delay, context.state.frame_count);
            set_fps(context, frame_delay);
        }
        if (context.state.video_packet_process_force_24fps && context.state.video_packet_process_find_24fps == 5)
        {
            frame_delay=0.0416666666666667;
            Debug(context, 1 ,"Framerate forced %6.3f fps at frame %d\n", 1.0/frame_delay, context.state.frame_count);
            set_fps(context, frame_delay);
        }

//#define SHOW_VIDEO_TIMING
#ifdef SHOW_VIDEO_TIMING
        if (framenum==0)
            Debug(1,"Video timing ---------------------------------------------------\n", frame_delay/is->ticks_per_frame, is->ticks_per_frame, repeat, real_pts,calculated_delay);
        else if (framenum<20)
            Debug(1,"Video timing fr=%6.5f, tick=%d, repeat=%d, pts=%6.3f, step=%6.5f\n", frame_delay/is->ticks_per_frame, is->ticks_per_frame, repeat, real_pts,calculated_delay);
#endif // SHOW_VIDEO_TIMING


        context.state.pts_offset *= 0.9;
        if (!context.state.reviewing && context.settings.timeline_repair) {
            if (context.state.framenum > 1 && fabs(calculated_delay - context.state.pts_offset - frame_delay) < 1.0) { // Allow max 0.5 second timeline jitter to be compensated
                if (!ISSAME(3*frame_delay/ is->ticks_per_frame, calculated_delay))
                    if (!ISSAME(1*frame_delay/ is->ticks_per_frame, calculated_delay))
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
            && !ISSAME(3*frame_delay/ is->ticks_per_frame, calculated_delay)
            && !ISSAME(2*frame_delay/ is->ticks_per_frame, calculated_delay)
            && !ISSAME(1*frame_delay/ is->ticks_per_frame, calculated_delay)
            ){
            if ( (context.state.video_packet_process_prev_strange_framenum + 1 != context.state.framenum) &&( context.state.video_packet_process_prev_strange_step < fabs(calculated_delay - frame_delay))) {
                Debug(context, 8 ,"Strange video pts step of %6.5f instead of %6.5f at frame %d\n", calculated_delay+0.0000005, frame_delay+0.0000005, context.state.framenum); // Unknown strange step
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
            DUMP_TIMING("v   set", real_pts, pts, is->video_clock, context.state.pts_offset, repeat);
        }
        else
        {
            /* if we aren't given a pts, set it to the clock */
            DUMP_TIMING("v clock", real_pts, pts, is->video_clock, context.state.pts_offset, repeat);
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
                    context.state.sample_file.reset(fopen("seektest.log", "a+"));
                    fprintf(context.state.sample_file.get(), "Seek error: target=%8.1f, result=%8.1f, error=%6.3f, size=%8.1f, mode=%s, \"%s\"\n",
                            is->seek_pts,
                            is->video_clock,
                            is->video_clock - is->seek_pts,
                            is->duration,
                            (is->seek_by_bytes ? "byteseek": "timeseek" ),
                            is->filename);
                    context.state.sample_file.reset();
                        Debug(context,  1,"\nSelftest 1 FAILED: Seektest\n:Starting test 3\n");
                   }
                    else
                        Debug(context,  1,"\nSelftest 1 OK: Seektest\nStarting test 3\n");
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
                    goto quit;
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
                        context.state.sample_file.reset(fopen("seektest.log", "a+"));
                        fprintf(context.state.sample_file.get(), "Reopen error: target=%8.1f, result=%8.1f, error=%6.3f, size=%8.1f, mode=%s, \"%s\"\n",
                            is->seek_pts,
                            is->video_clock,
                            is->video_clock - is->seek_pts,
                            is->duration,
                            (is->seek_by_bytes ? "byteseek": "timeseek" ),
                            is->filename);
                        context.state.sample_file.reset();
                        Debug(context,  1,"\nSelftest 3 FAILED: Reopen\n");
                    }
                    else
                        Debug(context,  1,"\nSelftest 3 OK: Reopen\n");
                    comskip::request_exit(1);
                }
                context.state.retries = 0;
                if (SubmitFrame (context, is->video_st, is->pFrame.get(), is->video_clock))
                {
                    goto quit;
                }
            } else {
                if (fabs(is->seek_pts - is->video_clock) > 80 ) {
                    Debug(context, 1,"Positioning file failing with pts=%6.2f\n", is->video_clock );
                    if (context.state.selftest == 1 || context.state.selftest == 3)
                    {
                        context.state.sample_file.reset(fopen("seektest.log", "a+"));
                        fprintf(context.state.sample_file.get(), "Seek error : target=%8.1f, result=%8.1f, error=%6.3f, size=%8.1f, mode=%s, \"%s\"\n",
                            is->seek_pts,
                            is->video_clock,
                            is->video_clock - is->seek_pts,
                            is->duration,
                            (is->seek_by_bytes ? "byteseek": "timeseek" ),
                            is->filename);
                        context.state.sample_file.reset();
                        Debug(context,  1,"\nSelftest %d FAILED\n", context.state.selftest);
                        comskip::request_exit(1);
                    }
                    goto quit;          //Temporary till the seek error is fixed.
                    context.state.retries = 0;
                    comskip::request_exit(-1);
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
                    context.captions->consume({sd->data, sd->size}, caption_timestamp(pts));
                for (const auto& packet : comskip::media::bridge_a53_captions({sd->data, sd->size})) {
                    std::copy_n(packet.bytes.begin(), packet.size, context.state.ccData);
                    context.state.ccDataLen = static_cast<int>(packet.size);
                    dump_data(context, reinterpret_cast<char*>(context.state.ccData), context.state.ccDataLen);
                    if (context.state.processCC) ProcessCCData(context);
                }
            }
        }
#endif
    }

    return frameFinished;
quit:
    return 0;
}


//extern int dxva2_init(AVCodecContext *s);


int stream_component_open(RecordingContext& context, VideoState *is, int stream_index)
{
    AVFormatContext *pFormatCtx = is->pFormatCtx.get();
    AVCodecParameters *codecPar = NULL;
    AVCodecContext *codecCtx;
    const AVCodec *codec;
    const AVCodec *codec_hw = NULL;



    if(stream_index < 0 || (unsigned int)stream_index >= pFormatCtx->nb_streams)
    {
        return -1;
    }

    if (strcmp(pFormatCtx->iformat->name, "mpegts")==0)
        context.state.demux_pid = 1;

    // Get a pointer to the codec context for the video stream

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

    // If decoding in hardware try if running on a Raspberry Pi and then use it's decoder instead.
    if (context.settings.hardware_decode && !codec_hw) {
        if (codecPar->codec_id == AV_CODEC_ID_MPEG2VIDEO) codec_hw = avcodec_find_decoder_by_name("mpeg2_mmal");
        if (codecPar->codec_id == AV_CODEC_ID_H264) codec_hw = avcodec_find_decoder_by_name("h264_mmal");
        if (codecPar->codec_id == AV_CODEC_ID_MPEG4) codec_hw = avcodec_find_decoder_by_name("mpeg4_mmal");
        if (codecPar->codec_id == AV_CODEC_ID_VC1) codec_hw = avcodec_find_decoder_by_name("vc1_mmal");
    }


    if (codec_hw != NULL && codec_hw != codec) {
        fputs(context.translator.format("media_using_codec", codec_hw->name, codec->name).c_str(), stderr);
        codec = codec_hw;
    }

    CodecPtr codec_owner(avcodec_alloc_context3(codec));
    codecCtx = codec_owner.get();
    if (!codecCtx) throw std::bad_alloc();
    avcodec_parameters_to_context(codecCtx, codecPar);

    if (codecCtx->codec_type == AVMEDIA_TYPE_VIDEO)
    {
        if (!context.settings.hardware_decode) codecCtx->flags |= AV_CODEC_FLAG_GRAY;


//        codecCtx->flags2 |= CODEC_FLAG2_FAST /* | AV_CODEC_FLAG2_SHOW_ALL */ ;
//        codecCtx->flags2 |= AV_CODEC_FLAG2_CHUNKS /* | AV_CODEC_FLAG2_SHOW_ALL */ ;



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
            Debug(0, "h.264 video can only be processed at full speed by the Donator version\n");
#endif
        }
        else
        {
#ifdef DONATOR
            int w;
            if (context.settings.lowres == 10) {
                w = codecCtx->width;
                context.settings.lowres = 0;
                while (w > 600) {
                    w = w >> 1;
                    context.settings.lowres++;
                }
            }
 //           codecCtx->lowres = lowres;
#endif
//            /* if(lowres) */ codecCtx->flags |= CODEC_FLAG_EMU_EDGE;
        }
//        codecCtx->flags2 |= CODEC_FLAG2_FAST;

        if (codecCtx->codec_id != AV_CODEC_ID_MPEG1VIDEO) {
#ifdef DONATOR
           codecCtx->thread_count= context.settings.thread_count;
#else
            codecCtx->thread_count= 1;
#endif
        }
    }

    if (!context.settings.hardware_decode) av_dict_set_int(std::inout_ptr(context.state.myoptions), "gray", 1, 0);


 //       av_dict_set_int(std::inout_ptr(myoptions), "fastint", 1, 0);
 //       av_dict_set_int(std::inout_ptr(myoptions), "skip_alpha", 1, 0);
//        av_dict_set(std::inout_ptr(myoptions), "threads", "auto", 0);

    if(!codec || (avcodec_open2(codecCtx, codec, std::inout_ptr(context.state.myoptions)) < 0))
    {
        fputs(context.translator.text("media_unsupported_codec"), stderr);
        return -1;
    }

    switch(codecCtx->codec_type)
    {
    case AVMEDIA_TYPE_SUBTITLE:
        is->subtitleStream = stream_index;
        is->subtitle_st = pFormatCtx->streams[stream_index];
        is->subtitle_ctx = std::move(codec_owner);
        if (context.state.demux_pid)
            context.state.selected_subtitle_pid = is->subtitle_st->id;
        break;
    case AVMEDIA_TYPE_AUDIO:
        is->audioStream = stream_index;
        is->audio_st = pFormatCtx->streams[stream_index];
        is->audio_ctx = std::move(codec_owner);
//          is->audio_buf_size = 0;
//          is->audio_buf_index = 0;

        /* averaging filter for audio sync */
//          is->audio_diff_avg_coef = exp(log(0.01 / AUDIO_DIFF_AVG_NB));
//          is->audio_diff_avg_count = 0;
        /* Correct audio only if larger error than this */
//          is->audio_diff_threshold = 2.0 * SDL_AUDIO_BUFFER_SIZE / codecCtx->sample_rate;
        if (context.state.demux_pid)
            context.state.selected_audio_pid = is->audio_st->id;


 //       memset(&is->audio_pkt, 0, sizeof(is->audio_pkt));
        break;
    case AVMEDIA_TYPE_VIDEO:
        is->videoStream = stream_index;
        is->video_st = pFormatCtx->streams[stream_index];
        is->dec_ctx = std::move(codec_owner);

//          is->frame_timer = (double)av_gettime() / 1000000.0;
//          is->frame_last_delay = 40e-3;
//          is->video_current_pts_time = av_gettime();

        is->pFrame = make_frame();
        if (!context.settings.hardware_decode) codecCtx->flags |= AV_CODEC_FLAG_GRAY;
//       codecCtx->thread_type = 1; // Frame based threading
        codecCtx->lowres = min(codecCtx->codec->max_lowres, context.settings.lowres);
        if (codecCtx->codec_id == AV_CODEC_ID_H264)
        {
            context.state.is_h264 = 1;
#ifdef DONATOR
#else
            Debug(0, "h.264 video can only be processed at full speed by the Donator version\n");
#endif
        }

        //        codecCtx->flags2 |= CODEC_FLAG2_FAST;
        if (codecCtx->codec_id != AV_CODEC_ID_MPEG1VIDEO) {
#ifdef DONATOR
           codecCtx->thread_count= context.settings.thread_count;
#else
            codecCtx->thread_count= 1;
#endif
        }
        // Mirrors what FFmpeg's own decoders used to set on AVCodecContext
        // before ticks_per_frame was removed: 2 for MPEG2's field-time
        // timebase convention, 1 for MPEG1's frame-time one.
        is->ticks_per_frame = (codecCtx->codec_id == AV_CODEC_ID_MPEG1VIDEO) ? 1 : 2;
        if (context.state.demux_pid)
            context.state.selected_video_pid = is->video_st->id;
        /*
        MPEG
                        if(  (codecCtx->skip_frame >= AVDISCARD_NONREF && s2->pict_type==FF_B_TYPE)
                            ||(codecCtx->skip_frame >= AVDISCARD_NONKEY && s2->pict_type!=FF_I_TYPE)
                            || codecCtx->skip_frame >= AVDISCARD_ALL)


                        if(  (s->avctx->skip_idct >= AVDISCARD_NONREF && s->pict_type == FF_B_TYPE)
                           ||(codecCtx->skip_idct >= AVDISCARD_NONKEY && s->pict_type != FF_I_TYPE)
                           || s->avctx->skip_idct >= AVDISCARD_ALL)
        h.264
            if(   s->codecCtx->skip_loop_filter >= AVDISCARD_ALL
               ||(s->codecCtx->skip_loop_filter >= AVDISCARD_NONKEY && h->slice_type_nos != FF_I_TYPE)
               ||(s->codecCtx->skip_loop_filter >= AVDISCARD_BIDIR  && h->slice_type_nos == FF_B_TYPE)
               ||(s->codecCtx->skip_loop_filter >= AVDISCARD_NONREF && h->nal_ref_idc == 0))

        Both
                        if(  (codecCtx->skip_frame >= AVDISCARD_NONREF && s2->pict_type==FF_B_TYPE)
                            ||(codecCtx->skip_frame >= AVDISCARD_NONKEY && s2->pict_type!=FF_I_TYPE)
                            || codecCtx->skip_frame >= AVDISCARD_ALL)
                            break;

        */
        if (context.settings.skip_B_frames)
            codecCtx->skip_frame = AVDISCARD_NONREF;
        //          codecCtx->skip_loop_filter = AVDISCARD_NONKEY;
//           codecCtx->skip_idct = AVDISCARD_NONKEY;

        break;
    default:
        break;
    }

    return(0);
}

/* av_dict_set(&options, "video_size", "640x480", 0);
 * if (avformat_open_input(&s, url, NULL, &options) < 0)
 *     abort();
 * av_dict_free(&options);
 */

void file_open(RecordingContext& context)
{
    VideoState *is;
    int subtitle_index= -1, audio_index= -1, video_index = -1;
    int openretries = 0;

    if (context.state.video_owner.get() == NULL)
    {
        context.state.video_owner = std::make_unique<VideoState>();
        is = context.state.video_owner.get();
        memset(&is->audio_pkt, 0, sizeof(is->audio_pkt));
        strcpy(is->filename, context.state.mpegfilename);
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
            av_dict_set_int(std::inout_ptr(myoptions), "threads", 1, 0);
//            codecCtx->thread_count= 1;
#endif
        av_dict_set_int(std::inout_ptr(context.state.myoptions), "refcounted_frames", 1, 0); // No need to keep multiple buffers


    }
    else
        is = context.state.video_owner.get();
    // Open video file
    if ( is->pFormatCtx.get() == NULL)
    {
        is->pFormatCtx.reset(avformat_alloc_context());
        if (!is->pFormatCtx) throw std::bad_alloc();
        is->pFormatCtx->max_analyze_duration *= 4;
//        pFormatCtx->probesize = 400000;
again:
        if(avformat_open_input(std::inout_ptr(is->pFormatCtx), is->filename, NULL,std::inout_ptr(context.state.myoptions))!=0)
        {
            fputs(context.translator.format("media_open_failed", is->filename).c_str(), stderr);
            if (openretries++ < context.settings.live_tv_retries)
            {
                sleep_for_ms(1000L);
                goto again;
            }
            comskip::request_exit(-1);

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
        if(avformat_find_stream_info(is->pFormatCtx.get(), 0L )<0)
        {
            fputs(context.translator.format("media_stream_info_failed", is->filename).c_str(), stderr);
            comskip::request_exit(-1);
        }
        // Dump information about file onto standard error
        if (context.state.retries == 0) av_dump_format(is->pFormatCtx.get(), 0, is->filename, 0);
    }

    if (!is->frame.get()) {
        if (!(is->frame = make_frame()))
            comskip::request_exit(-1);
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
            Debug(context, 0, "Could not open video codec\n");
            fputs(context.translator.format("media_video_codec_failed", is->filename).c_str(), stderr);
            comskip::request_exit(-1);
        }

        if ( is->video_st->duration == AV_NOPTS_VALUE ||  is->video_st->duration < 0)
            is->duration =  ((float)is->pFormatCtx->duration) / AV_TIME_BASE;
        else
            is->duration =  av_q2d(is->video_st->time_base)* is->video_st->duration;

        if (is->duration < 0 && (context.settings.live_tv_retries > 0)) {
           Debug(context, 0, "Could not establish duration, live TV decoding may fail\n");
        }


        /* Calc FPS */
        if(is->video_st->r_frame_rate.den && is->video_st->r_frame_rate.num)
        {
            is->fps = av_q2d(is->video_st->r_frame_rate);
        }
        else
        {
            Debug(context, 10, "Warning, no stream frame rate, deriving from codec\n");
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
                Debug(context, 1,"Could not open audio decoder or no audio present\n");
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
//                    DUMP_CLOSE
//                    DUMP_OPEN
//                    DUMP_HEADER
//                    close_data();
#ifdef PROCESS_CC
#endif

}



void file_close(RecordingContext& context)
{


//    av_freep(&ist->hwaccel_device);


    if (context.state.video_owner->dec_ctx.get()) context.state.video_owner->dec_ctx.reset();
    context.state.video_owner->videoStream = -1;
//    avcodec_free_context(&is->pFormatCtx->streams[is->videoStream]->codec);

    if (context.state.video_owner->audio_ctx.get()) context.state.video_owner->audio_ctx.reset();
    context.state.video_owner->audioStream = -1;
    if (context.state.video_owner->subtitle_ctx.get())  context.state.video_owner->subtitle_ctx.reset();
    context.state.video_owner->subtitleStream = -1;
//    is->pFormatCtx.reset();


    context.state.video_owner->pFormatCtx.reset();

    context.state.video_owner->frame.reset();
    context.state.video_owner->pFrame.reset();
    context.state.video_owner->img_convert_ctx.reset();

    context.state.ac3_packet_index = 0;
    context.state.ac3_package_misalignment_count = 0;


//  global_video_state = NULL;
};


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
                    int empty_packet_count = 0;

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

        if (strstr(argv[0],"comskipGUI"))
            context.settings.output_debugwindow = 1;
        const comskip::platform::ScopedAnalysisPolicy analysis_policy(
            strstr(argv[0], "comskipGUI") == nullptr);
        auto executable_directory = std::filesystem::path(std::u8string_view(
            reinterpret_cast<const char8_t*>(argv[0]))).parent_path();
        if (executable_directory.empty()) executable_directory = ".";
        const auto directory_utf8 = executable_directory.u8string();
        comskip::checked_format(context.state.HomeDir, "%s",
            reinterpret_cast<const char*>(directory_utf8.c_str()));

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

//        DUMP_OPEN

        if (context.settings.output_timing)
        {
            sprintf(context.state.tempstring, "%s.timing.csv", context.state.inbasename);
            context.state.timing_file.reset(myfopen(context.state.tempstring, "w"));
            DUMP_HEADER
        }

        av_log_set_level(AV_LOG_INFO);
//        av_log_set_flags(AV_LOG_SKIP_REPEATED);
//

        packet = &(context.state.video_owner->audio_pkt);

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

                    DUMP_CLOSE
                    DUMP_OPEN
                    DUMP_HEADER
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
                    Debug(context,  9,"Retry t_pos=%" PRId64 ", l_pos=%" PRId64 ", t_pts=%" PRId64 ", l_pts=%" PRId64 "\n", last_packet_pos, packet->pos, last_packet_pts, packet->pts);
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
                if (ret == AVERROR_EOF || context.state.video_owner->pFormatCtx->pb->eof_reached)
                {
                    if (context.state.selftest == 3)   // Either simulated EOF or real EOF before REOPEN_TIME
                    {
                        if (context.state.retries > 0)
                        {
                            if (context.state.video_owner->video_clock < context.state.selftest_target - 0.05 || context.state.video_owner->video_clock > context.state.selftest_target + 0.05)
                            {
                                context.state.sample_file.reset(fopen("seektest.log", "a+"));
                                fprintf(context.state.sample_file.get(), "\"%s\": reopen file failed, size=%8.1f, pts=%6.2f\n", context.state.video_owner->filename, context.state.video_owner->duration, context.state.video_owner->video_clock );
                                context.state.sample_file.reset();
                                Debug(context,  1,"\nSelftest %d FAILED\n", context.state.selftest);
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
                            Debug(context,  1,"\nSelftest %d starting: Reopen\n", context.state.selftest);
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
                        Debug(context,  1,"\nRetry=%d at frame=%d, time=%8.2f seconds\n", context.state.retries, context.state.framenum, retry_target);
                        Debug(context,  9,"Retry target pos=%" PRId64 ", pts=%" PRId64 "\n", last_packet_pos, last_packet_pts);

                        if (context.state.selftest == 0) sleep_for_ms(4000L);
                        file_open(context);
                        Set_seek(context, context.state.video_owner.get(), retry_target);

                        context.state.retries++;
                        goto again;
                    }

                    // Frame-threaded decoders retain output until an explicit
                    // end-of-input packet. Drain it before finalizing detection.
                    if (context.state.video_owner->dec_ctx.get())
                        video_packet_process(context, context.state.video_owner.get(), NULL);
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
                if (packet->size > 0 && packet->data != NULL)
                    video_packet_process(context, context.state.video_owner.get(), packet);
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
            if (context.state.video_owner->video_clock == old_clock)
            {
                empty_packet_count++;
                if (empty_packet_count > 1000)
                    Debug(context, 0, "Empty input\n");
                empty_packet_count = 0;
            }
            else
            {
                old_clock = context.state.video_owner->video_clock;
                empty_packet_count = 0;
            }
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
                context.state.sample_file.reset(fopen("seektest.log", "a+"));
                fprintf(context.state.sample_file.get(), "Seek error: target=%8.1f, result=%8.1f, error=%6.3f, size=%8.1f, mode=%s\"%s\"\n",
                        context.state.video_owner->seek_pts,
                        context.state.video_owner->video_clock,
                        context.state.video_owner->video_clock - context.state.video_owner->seek_pts,
                        context.state.video_owner->duration,
                        (context.state.video_owner->seek_by_bytes ? "byteseek": "timeseek" ),
                        context.state.video_owner->filename);
                context.state.sample_file.reset();
            } else
                Debug(context,  1,"\nSelftest 1 OK: Seektest\n");

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
            context.captions->finish(caption_timestamp(context.state.video_owner->video_clock));
            context.captions.reset();
        }

        tfps = print_fps (context, 1);

        Debug(context,  10,"\nParsed %d video frames and %d audio frames at %8.2f fps\n", context.state.framenum, context.state.sound_frame_counter, tfps);
        Debug(context,  10,"\nMaximum Volume found is %d\n", context.state.max_volume_found);


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

                DUMP_CLOSE
                if (context.settings.output_timing)
                {
                    context.settings.output_timing = 0;
                }

#if defined(_WIN32) || defined(HAVE_SDL)
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
