#include "exit_requested.h"
#include "legacy_detection.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <stdexcept>
#include <string>

int CountSceneChanges(RecordingContext& context, int StartFrame, int EndFrame)
{
    int i;
    double p = 0;
    int count = 0;
    for (i = 0; i < context.state.schange_count; i++)
    {
        if ((context.state.schange[i].frame > StartFrame) && (context.state.schange[i].frame < EndFrame))
        {
            count++;
            p += (double)(100 - context.state.schange[i].percentage)  / (100 - context.state.schange_threshold);
        }
    }
    count = (int) p;

    return (count);
}

void Debug(RecordingContext& context, int level, const char * fmt, ...)
{
    if(context.settings.verbose < level) return;

    va_list ap;
    va_start(ap, fmt);
    std::string message;
    try {
        va_list measure;
        va_copy(measure, ap);
        const int length = std::vsnprintf(nullptr, 0, fmt, measure);
        va_end(measure);
        if (length < 0) throw std::runtime_error("Could not format diagnostic message");
        message.resize(static_cast<std::size_t>(length) + 1);
        const int written = std::vsnprintf(message.data(), message.size(), fmt, ap);
        if (written != length) throw std::runtime_error("Could not format diagnostic message");
        message.resize(static_cast<std::size_t>(length));
    } catch (...) {
        va_end(ap);
        throw;
    }
    va_end(ap);

    if (context.state.output_console) _cprintf("%s", message.c_str());

    const auto log_file = comskip::platform::own_file(myfopen(context.state.logfilename.c_str(), "a+"));
    if (log_file)
    {
        std::fwrite(message.data(), 1, message.size(), log_file.get());
    }


}

void InitLogoBuffers(RecordingContext& context)
{
    if (context.settings.num_logo_buffers <= 0)
        throw std::invalid_argument("Logo buffer count must be positive");
    context.state.ensure_pixel_buffers(true);
    const auto count = static_cast<std::size_t>(context.settings.num_logo_buffers);
    context.state.logoFrameNum.assign(count, 0);
    if (context.state.width == 0 && context.state.height == 0) return;
    const auto size = context.state.haslogo.size();
    if (context.state.logoFrameBuffer.size() != count || context.state.lwidth != context.state.width ||
        context.state.lheight != context.state.height) {
        std::vector<std::vector<unsigned char>> buffers(count, std::vector<unsigned char>(size));
        context.state.logoFrameBuffer.swap(buffers);
    }
    context.state.lwidth = context.state.width;
    context.state.lheight = context.state.height;
    context.state.logoFrameBufferSize = static_cast<int>(size);
    context.state.newestLogoBuffer = -1;
    context.state.oldestLogoBuffer = 0;
    context.state.logoBuffersFull = false;
}
void Init_XDS_block(RecordingContext& context);

void InitComSkip(RecordingContext& context)
{
    int i, j;
    context.state.ensure_pixel_buffers((context.settings.commDetectMethod & LOGO) != 0);
    context.state.min_brightness_found = 255;
    context.state.max_logo_gap = -1;
    context.state.max_nonlogo_block_length = -1;
    context.state.logo_overshoot = 0.0;
    for (i = 0; i < 256; i++) context.state.brightHistogram[i] = 0;
    for (i = 0; i < 256; i++) context.state.uniformHistogram[i] = 0;
    for (i = 0; i < 256; i++) context.state.volumeHistogram[i] = 0;
    for (i = 0; i < 256; i++) context.state.silenceHistogram[i] = 0;

    if (context.state.framearray)
    {
        if(!context.state.initialized)
        {
            context.state.max_frame_count = (int)(60 * 60 * context.settings.fps) + 1;
            context.state.frame.resize(context.state.max_frame_count + 2);
        }
        if (context.state.frame.empty())
        {
            Debug(context, 0, "Could not allocate memory for frame array\n");
            comskip::request_exit(10);
        }
    }

//	if (commDetectMethod & BLACK_FRAME) {
    if(!context.state.initialized)
    {
        context.state.max_black_count = 500;
        context.state.black.resize(context.state.max_black_count + 2);
    }
    if (context.state.black.empty())
    {
        Debug(context, 0, "Could not allocate memory for black frame array\n");
        comskip::request_exit(11);
    }
//	} else {
//		Debug(1, "ERROR: ComSkip cannot run without black frames.\n");
//		comskip::request_exit(100);
//	}

    if (context.settings.commDetectMethod & LOGO)
    {
        if(!context.state.initialized)
        {
            context.state.max_logo_block_count = 1000;
            context.state.logo_block.resize(context.state.max_logo_block_count + 2);
        }
        if (context.state.logo_block.empty())
        {
            Debug(context, 0, "Could not allocate memory for logo cblock array\n");
            comskip::request_exit(13);
        }

//		if (!logoInfoAvailable) {
        InitLogoBuffers(context);
//		}
        std::ranges::fill(context.state.max_br, 0);
        std::ranges::fill(context.state.min_br, 255);
    }

    if (context.settings.commDetectMethod & SCENE_CHANGE)
    {
        if(!context.state.initialized)
        {
            context.state.max_schange_count = 2000;
            context.state.schange.resize(context.state.max_schange_count + 2);
        }
        if (context.state.schange.empty())
        {
            Debug(context, 0, "Could not allocate memory for scene change array\n");
            comskip::request_exit(12);
        }
    }

    if (context.state.processCC)
    {
        if(!context.state.initialized)
        {
            context.state.max_cc_block_count = 500;
            context.state.cc_block.resize(context.state.max_cc_block_count + 2);
        }
        if (context.state.cc_block.empty())
        {
            Debug(context, 0, "Could not allocate memory for cc blocks\n");
            comskip::request_exit(22);
        }

        context.state.cc_block[0].start_frame = 0;
        context.state.cc_block[0].end_frame = -1;
        context.state.cc_block[0].type = NONE;
        for (i = 1; i < context.state.max_cc_block_count; i++)
        {
            context.state.cc_block[i].start_frame = -1;
            context.state.cc_block[i].end_frame = -1;
            context.state.cc_block[i].type = NONE;
        }

        context.state.cc_memory = {};
        context.state.cc_screen = {};

        if(!context.state.initialized)
        {
            context.state.max_cc_text_count = 1;
            context.state.cc_text.resize(context.state.max_cc_text_count + 2);
        }
        if (context.state.cc_text.empty())
        {
            Debug(context, 0, "Could not allocate memory for cc text groups\n");
            comskip::request_exit(22);
        }

        context.state.cc_text[0].start_frame = 1;
        context.state.cc_text[0].end_frame = -1;
        context.state.cc_text[0].text[0] = '\0';
        context.state.cc_text[0].text_len = 0;
        for (i = 1; i < context.state.max_cc_text_count; i++)
        {
            context.state.cc_text[i].start_frame = -1;
            context.state.cc_text[i].end_frame = -1;
            context.state.cc_text[i].text[0] = '\0';
            context.state.cc_text[i].text_len = 0;
        }
    }

//	if (commDetectMethod & AR) {
    if(!context.state.initialized)
    {
        context.state.max_ar_block_count = 100;
        context.state.ar_block.resize(context.state.max_ar_block_count + 2);
        context.state.max_ac_block_count = 100;
        context.state.ac_block.resize(context.state.max_ac_block_count + 2);
    }
    if (context.state.ar_block.empty())
    {
        Debug(context, 0, "Could not allocate memory for aspect ratio block array\n");
        comskip::request_exit(31);
    }
    if (context.state.ac_block.empty())
    {
        Debug(context, 0, "Could not allocate memory for audio channel block array\n");
        comskip::request_exit(31);
    }
//	}

    context.state.cc.cc1[0] = 0;
    context.state.cc.cc1[1] = 0;
    context.state.cc.cc2[0] = 0;
    context.state.cc.cc2[1] = 0;
    context.state.lastcc.cc1[0] = 0;
    context.state.lastcc.cc1[1] = 0;
    context.state.lastcc.cc2[0] = 0;
    context.state.lastcc.cc2[1] = 0;

    Init_XDS_block(context);

    if (context.settings.max_avg_brightness == 0)
    {
        if (context.settings.fps == 25.00)
            context.settings.max_avg_brightness = 19;
        else
            context.settings.max_avg_brightness = 19;
    }
    context.state.schange_count = 0;
    context.state.frame_count	= 0;
    context.state.framesprocessed =0;
    context.state.black_count = 0;
    context.state.block_count = 0;
    context.state.ar_block_count = 0;
    context.state.ac_block_count = 0;
    context.state.framenum_real = 0;
    context.state.frames_with_logo = 0;
    context.state.framenum = 0;
    context.state.lastLogoTest = false;
    context.state.commercial_count = -1;

    context.state.logoTrendCounter = 0;
//	audio_framenum = 0;
    context.state.cc_block_count = 0;
    context.state.cc_text_count = 0;
    context.state.logo_block_count = 0;
//	pts = 0;
    context.state.ascr=context.state.scr=0;
    InitScanLines(context);
    InitHasLogo(context);
    context.state.initialized = true;
    close_dump(context);
}

