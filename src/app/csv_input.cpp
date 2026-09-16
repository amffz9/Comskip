#include "exit_requested.h"
#include "checked_format.h"
#include "legacy_detection.h"
#include <filesystem>
#include <stdexcept>
#include <limits>
#include "input/file_stream.h"
#include "input/frame_record.h"
#include "input/reference_file.h"
#include "input/checked_number.h"

void PrintArgs(RecordingContext& context)
{
    for (std::size_t i = 0; i < context.state.argument.size(); ++i)
        printf("%zu\t%s\n", i, context.state.argument[i].c_str());
}




void ProcessCSV(RecordingContext& context, comskip::platform::FilePtr input)
{
    bool lastLogoTest = false, curLogoTest = false;
    char line[2048]{}; // Bounded persisted caption framing, independent of CSV.
    int cont = 0;
    int minminY = 10000, maxmaxY = 0, minminX = 10000, maxmaxX = 0;
    int cutscene_nonzero_count = 0, old_format = true, use_bright = 0;
    int i, ccDataFrame;
    if (!input) throw std::invalid_argument("Missing CSV input");
    comskip::input::FileStreamBuffer buffer(input.get());
    std::istream source(&buffer);
again:
    auto header = comskip::input::read_text_line(source);
    if (!header) throw std::invalid_argument("CSV input has no header");
    if (comskip::input::trim_ascii(*header) == "sep=," || comskip::input::trim_ascii(*header) == "sep=;") {
        header = comskip::input::read_text_line(source);
        if (!header) throw std::invalid_argument("CSV input has no column header");
    }
    const auto rate = comskip::input::parse_frame_rate(*header);
    const double frame_rate = rate.value_or(context.settings.fps);
    std::optional<double> previous_time;
    std::vector<comskip::input::FrameRecord> observations;
    while (const auto text = comskip::input::read_text_line(source)) {
        const auto record = comskip::input::parse_frame_record(*text);
        if (observations.size() >= static_cast<std::size_t>(std::numeric_limits<int>::max() - 2))
            throw std::length_error("CSV observation count exceeds the frame index range");
        if (record.number != static_cast<int>(observations.size() + 1))
            throw std::invalid_argument("CSV frame numbers must be consecutive from one");
        if (record.min_y < 0 || record.min_x < 0 || record.max_y < record.min_y || record.max_x < record.min_x ||
            record.max_y > MAXHEIGHT || record.max_x > MAXWIDTH)
            throw std::invalid_argument("CSV observation has invalid scan bounds");
        const double timestamp = record.timestamp.value_or((record.number - 1) / frame_rate);
        if (!std::isfinite(timestamp) || timestamp > static_cast<double>(std::numeric_limits<std::int64_t>::max()) / 1e6 - 1 / frame_rate ||
            (previous_time && timestamp < *previous_time))
            throw std::invalid_argument("CSV timestamps must be representable and monotonic");
        previous_time = timestamp;
        observations.push_back(record);
    }
    if (observations.empty()) throw std::invalid_argument("CSV input has no observations");
    // Validate all syntax and indices before changing recording settings/state.
    if (rate) context.settings.fps = *rate;
    context.state.logoInfoAvailable = true;
    InitComSkip(context);
    context.state.frame_count = 1;
    context.state.pict_type = '?';
    for (const auto& record : observations) {
        InitializeFrameArray(context, record.number);
        auto& frame = context.state.frame[record.number];
        frame.brightness = record.brightness; frame.schange_percent = record.scene_change;
        frame.logo_present = record.logo; frame.uniform = record.uniform; frame.volume = record.volume;
        frame.minY = record.min_y; frame.maxY = record.max_y;
        frame.ar_ratio = record.aspect_ratio; frame.currentGoodEdge = record.good_edge;
        frame.isblack = record.black; frame.cutscenematch = record.cutscene_match;
        frame.minX = record.min_x; frame.maxX = record.max_x;
        frame.hasBright = record.bright_count; frame.dimCount = record.dim_count;
        frame.pts = record.timestamp.value_or((record.number - 1) / context.settings.fps);
        frame.cur_segment = record.segment; frame.audio_channels = record.audio_channels;
        frame.pict_type = '?';
        minminY = std::min(minminY, record.min_y); maxmaxY = std::max(maxmaxY, record.max_y);
        minminX = std::min(minminX, record.min_x); maxmaxX = std::max(maxmaxX, record.max_x);
        if (record.black != 0 && record.black != 1) old_format = false;
        if (record.cutscene_match > 0) ++cutscene_nonzero_count;
        ++context.state.frame_count;
    }
    observations.clear();
    context.state.frame[0].pts = context.state.frame[1].pts;
    context.state.frame[context.state.frame_count].pts = (context.state.frame_count - 1) / context.settings.fps;
    if (!context.state.dump_data_file.get())
    {
        auto companion = std::filesystem::path(std::u8string_view(
            reinterpret_cast<const char8_t*>(context.state.inbasename.c_str())));
        companion += ".data";
        const auto name = companion.u8string();
        context.state.dump_data_file.reset(myfopen(reinterpret_cast<const char*>(name.c_str()), "rb"));
        if (!context.state.dump_data_file && context.state.inbasename != context.state.workbasename) {
            const auto alternate_name = context.state.workbasename + ".data";
            context.state.dump_data_file.reset(myfopen(alternate_name.c_str(), "rb"));
        }
    }
    ccDataFrame = 0;


    for (i=0; i < 1000 && i < context.state.frame_count; i++)
    {
        if (context.state.frame[i].hasBright > 0)
            use_bright = 1;
    }


    context.state.height = maxmaxY + minminY;
    context.state.videowidth = context.state.width = maxmaxX + minminX;

    context.state.last_brightness = context.state.frame[1].brightness;
    Debug(context, 8, "CSV file loaded into memory.\n");
    input.reset();

    context.state.black_count = 0;
    context.state.logo_block_count = 0;
    context.state.black_count = 0;
    context.state.schange_count = 0;
    context.state.min_brightness_found = 255;
    for (i = 1; i < context.state.frame_count; i++)
    {
        context.state.framenum_real = i;
ccagain:
        if (context.state.dump_data_file.get() && ccDataFrame == 0)
        {
            const auto bytes_read = fread(line, 1, 8, context.state.dump_data_file.get());
            cont = bytes_read != 0;
            if (bytes_read && bytes_read != 8) throw std::invalid_argument("Truncated persisted caption frame header");
            if (bytes_read) {
                line[8] = 0;
                if (line[7] != ':' || sscanf(line, "%7d", &ccDataFrame) != 1 || ccDataFrame < 0)
                    throw std::invalid_argument("Invalid persisted caption frame header");
            }
//			ccDataFrame = strtol(line,NULL,7);
        }
        if (context.state.dump_data_file.get() )
        {

            while (cont && ccDataFrame <=i)
            {

                if (fread(line, 1, 4, context.state.dump_data_file.get()) != 4)
                    throw std::invalid_argument("Truncated persisted caption packet length");
                line[4]=0;
                if (sscanf(line,"%4d",&context.state.ccDataLen) != 1 || context.state.ccDataLen < 0 ||
                    context.state.ccDataLen > static_cast<int>(sizeof(context.state.ccData)))
                    throw std::invalid_argument("Invalid persisted caption packet length");
//			ccDataLen = strtol(line,NULL,4);
                if (context.state.ccDataLen && fread(context.state.ccData, 1, context.state.ccDataLen,
                    context.state.dump_data_file.get()) != static_cast<std::size_t>(context.state.ccDataLen))
                    throw std::invalid_argument("Truncated persisted caption packet");
                context.state.framenum = ccDataFrame;
#ifdef PROCESS_CC
                if (context.state.processCC) ProcessCCData(context);
                if (context.captions) context.captions->consume_stored_packet(
                    {context.state.ccData, static_cast<std::size_t>(context.state.ccDataLen)},
                    std::chrono::duration_cast<comskip::media::CaptionTimestamp>(
                        std::chrono::duration<double>(context.state.frame[i].pts)));
#endif
                ccDataFrame = 0;
                goto ccagain;
            }

        }
        if (old_format)
        {
            if (context.state.frame[i].isblack)
            {
                if (context.state.frame[i].brightness <= 5)
                {
                    if (context.state.frame[i].brightness == 5)
                        context.state.frame[i].isblack = C_a;
                    if (context.state.frame[i].brightness == 4)
                        context.state.frame[i].isblack = C_u;         // Checked
                    if (context.state.frame[i].brightness == 3)
                        context.state.frame[i].isblack = C_s;
                    if (context.state.frame[i].brightness == 2)
                        context.state.frame[i].isblack = C_s;			// Checked
                    if (context.state.frame[i].brightness == 1)
                        context.state.frame[i].isblack = C_u;
                    context.state.frame[i].brightness = context.settings.max_avg_brightness + 1;
                }
                else
                    context.state.frame[i].isblack = C_b;
            }
            else
                context.state.frame[i].isblack = 0;
        }
        else
        {
//			frame[i].isblack &= C_b;
        }

        if (context.state.frame[i].brightness > 0)
        {
            context.state.brightHistogram[std::clamp(context.state.frame[i].brightness, 0, 255)]++;

            if (context.state.frame[i].brightness < context.state.min_brightness_found) context.state.min_brightness_found = context.state.frame[i].brightness;

            context.state.uniformHistogram[std::clamp(context.state.frame[i].uniform / UNIFORMSCALE, 0, 255)]++;
        }

        if (context.state.frame[i].volume >= 0)
        {
            context.state.volumeHistogram[(context.state.frame[i].volume/context.state.volumeScale < 255 ? context.state.frame[i].volume/context.state.volumeScale : 255)]++;
            context.state.silenceHistogram[(context.state.frame[i].volume < 255 ? context.state.frame[i].volume : 255)]++;
        }


        if (context.state.frame[i].maxX == 0)
        {
            if (i == 1)
            {
                if (context.state.frame[i].ar_ratio < 0.5)
                {
                    if (context.state.frame[i].maxY + context.state.frame[i].minY < 600)
                        context.state.videowidth = context.state.width = 720;
                    else if (context.state.frame[i].maxY + context.state.frame[i].minY < 800)
                        context.state.videowidth = context.state.width = 1200;
                    else
                        context.state.videowidth = context.state.width = 1920;
                }
                else
                    context.state.videowidth = context.state.width = (int) ((context.state.frame[i].maxY + context.state.frame[i].minY) * context.state.frame[i].ar_ratio );
            }
            context.state.frame[i].maxX = context.state.videowidth - 10;
            context.state.frame[i].minX = 10;
        }
        if (i == 1)
        {
//			if (frame[i].maxX == 0)
//				videowidth = width = (int) ((frame[i].maxY - frame[i].minY) * frame[i].ar_ratio );
            ProcessARInfoInit(context, context.state.frame[i].minY, context.state.frame[i].maxY, context.state.frame[i].minX, context.state.frame[i].maxX);
            ProcessACInfoInit(context, context.state.frame[i].audio_channels);
        }
        else
        {
            ProcessARInfo(context, context.state.frame[i].minY, context.state.frame[i].maxY, context.state.frame[i].minX, context.state.frame[i].maxX);
            ProcessACInfo(context, context.state.frame[i].audio_channels);
        }
        context.state.frame[i].ar_ratio = context.state.last_ar_ratio;


        if ((context.settings.commDetectMethod & RESOLUTION_CHANGE))
        {
            /* not reliable!!!!!!!!!!!!!!!!!!!!!
                        frame[i].isblack &= ~C_r;
                        videowidth = width = frame[i].minX + frame[i].maxX;
                        height = frame[i].minY + frame[i].maxY;

                        if ((old_width != 0 && abs(width-old_width) > 50) || (old_height != 0 && abs(height - old_height) > 50)) {
                            frame[i].isblack |= C_r;
                        }
                        old_width = width;
                        old_height = height;
            */
        }
        else
            context.state.frame[i].isblack &= ~C_r;

        if (context.settings.commDetectMethod & BLACK_FRAME)
        {
            // if (frame[i].brightness <= max_avg_brightness && (non_uniformity == 0 || frame[i].uniform < non_uniformity)/* && frame[i].volume < max_volume */ && !(frame[i].isblack & C_b))
            //    frame[i].isblack |= C_b;
            if ((context.state.frame[i].isblack & C_b) && context.state.frame[i].brightness > context.settings.max_avg_brightness)
                context.state.frame[i].isblack &= ~C_b;

            if (use_bright)
            {

                if (context.state.frame[i].hasBright > 0 && context.state.min_hasBright > context.state.frame[i].hasBright * 720 * 480 / context.state.videowidth / context.state.height) context.state.min_hasBright = context.state.frame[i].hasBright * 720 * 480 / context.state.videowidth / context.state.height;
                if (context.state.frame[i].dimCount > 0 && context.state.min_dimCount > context.state.frame[i].dimCount * 720 * 480 / context.state.videowidth / context.state.height) context.state.min_dimCount = context.state.frame[i].dimCount * 720 * 480 / context.state.videowidth / context.state.height;

                if (context.state.frame[i].brightness <= context.settings.max_avg_brightness && context.state.frame[i].hasBright < context.settings.maxbright && context.state.frame[i].dimCount < (int)(.05 * context.state.videowidth * context.state.height))
                    context.state.frame[i].isblack |= C_b;
            }
            if (i>1) { // Uniform not calculated for frame 1
                context.state.frame[i].isblack &= ~C_u;
                if (!(context.state.frame[i].isblack & C_b) && context.settings.non_uniformity > 0 && context.state.frame[i].uniform < context.settings.non_uniformity && context.state.frame[i].brightness < 250 /*&& frame[i].volume < max_volume*/ )
                    context.state.frame[i].isblack |= C_u;
            }
        }
        else
        {
            context.state.frame[i].isblack &= ~(C_u | C_b);
        }

        if (context.state.frame[i].isblack & C_s)
            context.state.frame[i].isblack &= ~C_s;


        if (context.settings.commDetectMethod & SCENE_CHANGE && !(context.state.frame[i-1].isblack & C_b) && !(context.state.frame[i].isblack & C_b))
        {
            if (context.state.frame[i].brightness > 5 && abs(context.state.frame[i].brightness - context.state.last_brightness) > context.settings.brightness_jump)
            {
                context.state.frame[i].isblack |= C_s;
            }
            if (context.state.frame[i].brightness > 5)
                context.state.last_brightness = context.state.frame[i].brightness;

            if (context.state.frame[i].brightness > 5 && context.state.frame[i].schange_percent < 15)
            {
                context.state.frame[i].isblack |= C_s;
            }
        }


        if (context.state.frame[i].isblack & C_t)
            context.state.frame[i].isblack &= ~C_t;

        if (context.settings.commDetectMethod & CUTSCENE && cutscene_nonzero_count > 0)
        {
            if (context.state.frame[i].cutscenematch < context.settings.cutscenedelta)
                context.state.frame[i].isblack |= C_t;
        }

        if (context.state.frame[i].isblack & C_v)
            context.state.frame[i].isblack &= ~C_v;

        if (context.settings.commDetectMethod & SILENCE)
        {
            if (0 <= context.state.frame[i].volume && context.state.frame[i].volume < context.settings.max_silence && context.settings.min_silence == 1)
            {
                context.state.frame[i].isblack |= C_v;
            }
            if (context.state.frame[i].volume < 6)
            {
                context.state.frame[i].isblack |= C_v;
            }
        }


        if (context.state.frame[i].isblack)
        {
            InsertBlackFrame(context, i,context.state.frame[i].brightness,context.state.frame[i].uniform,context.state.frame[i].volume, (int)context.state.frame[i].isblack);

            /*
                        j = i-volume_slip;
                        if (j < 0) j = 0;
                        k = i+volume_slip;
                        if (k>frame_count) k = frame_count;
                        for (x=j; x<k; x++)
                            if (frame[x].volume >= 0)
                                if (black[black_count].volume > frame[x].volume)
                                    black[black_count].volume = frame[x].volume;
            //			if (black[black_count].volume < max_volume) frame[i].volume = 1;
            */
        }

        if ((context.state.frame[i].schange_percent < 20) && i > 1 && context.state.black_count > 0 && (context.state.black[context.state.black_count - 1].frame != i))
        {
            if (context.state.frame[i].brightness < context.state.frame[i - 1].brightness * 2)
            {
                InitializeSchangeArray(context, context.state.schange_count);
                context.state.schange[context.state.schange_count].percentage = context.state.frame[i].schange_percent;
                context.state.schange[context.state.schange_count].frame = i;
                context.state.schange_count++;
            }
        }
        else if (context.state.frame[i].schange_percent < context.state.schange_threshold)
        {

            // Scene Change threshold: original = 91
            InitializeSchangeArray(context, context.state.schange_count);
            context.state.schange[context.state.schange_count].percentage = context.state.frame[i].schange_percent;
            context.state.schange[context.state.schange_count].frame = i;
            context.state.schange_count++;
        }

        if ((context.settings.commDetectMethod & LOGO) && ((i % (int)(context.settings.fps * context.state.logoFreq)) == 0))
        {
            curLogoTest = (context.state.frame[i].currentGoodEdge > context.settings.logo_threshold);
            lastLogoTest = ProcessLogoTest(context, i, curLogoTest, false);
            context.state.frame[i].logo_present = lastLogoTest;
        }
        if (lastLogoTest) context.state.frames_with_logo++;

//		if (live_tv && !frame[i].isblack) {
//			BuildCommListAsYouGo();
//		}
//        DetectCredits(i);

    }
    if (context.captions) {
        context.captions->finish(std::chrono::duration_cast<comskip::media::CaptionTimestamp>(
            std::chrono::duration<double>(context.state.frame[context.state.frame_count - 1].pts + 1 / context.settings.fps)));
        context.captions.reset();
    }
    // Parsing leaves count at the extra terminal slot. Detection and exports
    // use the decoder convention: count is the final real observation index.
    // Keep the allocated terminal slot, but exclude it from media statistics.
    --context.state.frame_count;
    context.state.framenum_real = context.state.frame_count;
    context.state.framesprocessed = context.state.frame_count;
    if (context.settings.output_live) {
        OutputBlackArray(context);
        BuildCommListAsYouGo(context);
    }
    BuildMasterCommList(context);

    if (context.settings.output_debugwindow)
    {
#ifdef DEBUG
        skip_B_frames=0;
#endif
#ifdef DONATOR
        context.settings.skip_B_frames=0;
#endif
        context.state.processCC = 0;
        i = 0;
        printf("Close window when done\n");
        if (ReviewResult(context))
        {
            LoadIniFile(context);
            goto again;
        }
        //		printf(" Press Enter to close debug window\n");
//		gets(HomeDir);
    }
    comskip::request_exit(0);
}

