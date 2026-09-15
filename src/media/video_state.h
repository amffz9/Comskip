#pragma once
#include "ffmpeg_resources.h"
using namespace comskip::media;
typedef struct VideoState

{

    NetworkSession network;
    InputPtr pFormatCtx;

    CodecPtr dec_ctx, audio_ctx, subtitle_ctx;

    // AVCodecContext.ticks_per_frame was removed in FFmpeg 8. It used to be

    // set by the decoder itself: 1 for MPEG1VIDEO, 2 for everything else this

    // code cared about (MPEG2's field-time timebase convention). Comskip

    // already overrode the MPEG1 case explicitly below, so that is the only

    // distinction that ever mattered here; this field reproduces it locally

    // instead of asking the decoder for a value it no longer exposes.

    int             ticks_per_frame;

    int             videoStream, audioStream, subtitleStream;



    int             av_sync_type;

//     double          external_clock; /* external clock base */

//     int64_t         external_clock_time;

    int             seek_req;

    int             seek_by_bytes;

    int             seek_no_flush;

    double           seek_pts;

    int             seek_flags;

    int64_t          seek_pos;

    double          audio_clock;

    AVStream        *audio_st;

    AVStream        *subtitle_st;



    //DECLARE_ALIGNED(16, uint8_t, audio_buf[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2]);

    unsigned int    audio_buf_size;

    unsigned int    audio_buf_index;

    AVPacket        audio_pkt;

    AVPacket        audio_pkt_temp;

//  uint8_t         *audio_pkt_data;

//  int             audio_pkt_size;

    int             audio_hw_buf_size;

    double          audio_diff_cum; /* used for AV difference average computation */

    double          audio_diff_avg_coef;

    double          audio_diff_threshold;

    int             audio_diff_avg_count;

    double          frame_timer;

    double          frame_last_pts;

    double          frame_last_delay;

    double          video_clock; ///<pts of last decoded frame / predicted pts of next decoded frame

    double          video_clock_submitted;

    double          video_current_pts; ///<current displayed pts (different from video_clock if frame fifos are used)

    int64_t         video_current_pts_time;  ///<time (av_gettime) at which we updated video_current_pts - used to have running video pts

    AVStream        *video_st;

    FramePtr pFrame;

    char            filename[1024];

    int             quit;

    FramePtr frame;

    double			 duration;

    double			 fps;

    ScalerPtr img_convert_ctx;

} VideoState;
