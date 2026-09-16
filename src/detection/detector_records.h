#pragma once
#include <cstdint>

// Per-recording detector observations and intervals. No application
// dependencies.

struct frame_info {
    int brightness;
    int schange_percent;
    int minY;
    int maxY;
    int uniform;
    int volume;
    double currentGoodEdge;
    double ar_ratio;
    bool logo_present;
    bool commercial;
    int isblack;
    int64_t goppos;
    double pts;
    char pict_type;
    int minX;
    int maxX;
    int hasBright;
    int dimCount;
    int cutscenematch;
    double logo_filter;
    int xds;
    int cur_segment;
    int audio_channels;
};

struct schange_info {
    long frame;
    int percentage;
};

struct black_frame_info {
    long frame;
    int brightness;
    long uniform;
    int volume;
    int cause;
};

struct block_info {
    long f_start;
    long f_end;
    unsigned int b_head;
    unsigned int b_tail;
    unsigned int bframe_count;
    unsigned int schange_count;
    double schange_rate; // in changes per second
    double length;
    double score;
    int combined_count;
    int cc_type;
    double ar_ratio;
    int audio_channels;
    int cause;
    int more;
    int less;
    int brightness;
    int volume;
    int silence;
    int uniform;
    int stdev;
    char reffer;
    double logo;
    double correlation;
    int strict;
    int iscommercial;
};

struct logo_block_info {
    int start;
    int end;
};

struct ccPacket {
    unsigned char cc1[2];
    unsigned char cc2[2];
};

struct cc_block_info {
    long start_frame;
    long end_frame;
    int type;
};

struct XDS_block_info {
    long frame;
    char name[40];
    int v_chip;
    int duration;
    int position;
    int composite1;
    int composite2;
};

struct cc_text_info {
    long start_frame;
    long end_frame;
    long text_len;
    unsigned char text[256];
};

struct ar_block_info {
    int start;
    int end;
    double ar_ratio;
    int volume;
    int height, width;
    int minX, maxX, minY, maxY;
};

struct ac_block_info {
    int start;
    int end;
    int audio_channels;
};

struct Legacy_commercial_entry {
    long start_frame;
    long end_frame;
    int start_block;
    int end_block;
    double length;
};

struct Legacy_reffer_entry {
    long start_frame;
    long end_frame;
};

struct Legacy_ar_histogram_entry {
    long frames;
    double ar_ratio;
};

struct Legacy_ac_histogram_entry {
    long frames;
    int audio_channels;
};
