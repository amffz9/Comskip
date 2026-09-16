#include "../localization/diagnostic.h"
#include "exit_requested.h"
#include "legacy_detection.h"
#include "buffer_growth.h"

#include <algorithm>
#include <format>
#include <iterator>
#include <utility>

namespace {
template <typename... Args>
void CaptionDebug(RecordingContext& context, int level, const char* key, Args&&... args)
{
    Debug(context, level, "%s", context.translator.format(key, std::forward<Args>(args)...).c_str());
}
std::string CaptionTypeText(RecordingContext& context, int type)
{
    if (!context.state.processCC)
        return {};
    switch (type) {
    case NONE: return context.translator.text("caption_type_none");
    case ROLLUP: return context.translator.text("caption_type_rollup");
    case PAINTON: return context.translator.text("caption_type_painton");
    case POPON: return context.translator.text("caption_type_popon");
    case COMMERCIAL: return context.translator.text("caption_type_commercial");
    default: return std::format("{}", type);
    }
}
}

void OutputCCBlock(RecordingContext& context, long i)
{
    if (i < 0)
        return;
    if (i > 1)
    {
        CaptionDebug(context, 11, "caption_block_line", std::format("{}", i - 2),
            std::format("{:6}", context.state.cc_block[i - 2].start_frame),
            std::format("{:6}", context.state.cc_block[i - 2].end_frame),
            CaptionTypeText(context, context.state.cc_block[i - 2].type));
    }

    if (i > 0)
    {
        CaptionDebug(context, 11, "caption_block_line", std::format("{}", i - 1),
            std::format("{:6}", context.state.cc_block[i - 1].start_frame),
            std::format("{:6}", context.state.cc_block[i - 1].end_frame),
            CaptionTypeText(context, context.state.cc_block[i].type));
    }

    if (i <= 0)
    {
        CaptionDebug(context, 11, "caption_block_line", std::format("{}", i),
            std::format("{:6}", context.state.cc_block[i].start_frame),
            std::format("{:6}", context.state.cc_block[i].end_frame),
            CaptionTypeText(context, context.state.cc_block[i - 1].type));
    }
}

void Init_XDS_block(RecordingContext& context)
{
    if(context.state.XDS_block.empty())
    {
        comskip::detection::grow_buffer(context.state.XDS_block,
            context.state.max_XDS_block_count, 0, 2000);
        context.state.XDS_block_count = 0;
        context.state.XDS_block[context.state.XDS_block_count].frame = 0;
        context.state.XDS_block[context.state.XDS_block_count].name[0] = 0;
        context.state.XDS_block[context.state.XDS_block_count].v_chip = 0;
        context.state.XDS_block[context.state.XDS_block_count].duration = 0;
        context.state.XDS_block[context.state.XDS_block_count].position = 0;
        context.state.XDS_block[context.state.XDS_block_count].composite1 = 0;
        context.state.XDS_block[context.state.XDS_block_count].composite2 = 0;
    }
}

void Add_XDS_block(RecordingContext& context)
{
    Init_XDS_block(context);
    auto& frame = context.state.frame.at(static_cast<std::size_t>(context.state.framenum));
    if (context.state.XDS_block_count < 0)
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::invalid_xds_block_index);
    if (context.state.XDS_block_count == std::numeric_limits<long>::max())
        throw comskip::diagnostics::DiagnosticError<std::length_error>(comskip::diagnostics::Code::too_much_xds_data);
    const long next = context.state.XDS_block_count + 1;
    comskip::detection::grow_buffer(context.state.XDS_block,
        context.state.max_XDS_block_count, next, 2000);
    context.state.XDS_block[next] = context.state.XDS_block[context.state.XDS_block_count];
    context.state.XDS_block[next].frame = context.state.framenum;
    frame.xds = next;
    context.state.XDS_block_count = next;
}










#define MAXXDSBUFFER	1024
void AddXDS(RecordingContext& context, unsigned char hi, unsigned char lo)
{


    int i,j;
    int newXDS = 0;
    const bool end_packet = (hi & 0x7f) == 0x0f;
    if (context.state.AddXDS_c < 0 || context.state.AddXDS_c > MAXXDSBUFFER - 2 ||
        (context.state.AddXDS_c % 2) != 0) {
        context.state.AddXDS_c = 0;
        context.state.startXDS = 1;
    }
    Init_XDS_block(context);
    if (context.state.startXDS)
    {
        if ((hi & 0x70) == 0 && !end_packet)
        {
            context.state.startXDS = 0;
            context.state.AddXDS_c = 0;
            std::fill(std::begin(context.state.AddXDS_XDSbuf), std::end(context.state.AddXDS_XDSbuf), 0);
            context.state.baseXDS = hi & 0x0f;;
        }
        else
            return;
    }
    else
    {
        if ((hi & 0x7f) == context.state.baseXDS + 1)
            return; // COntinueation code
        if ((hi & 0x70) == 0 && !end_packet)
            return;
    }
    if ((hi & 0x01) == 0 && (hi & 0x70) == 0x00 && !end_packet)
        return;
    if (hi == 0x86 && (lo == 0x02 || lo == 1))
        return;
    if (context.state.AddXDS_c >= MAXXDSBUFFER - 4)
    {
        for (i = 0; i < 256; i++)
            context.state.AddXDS_XDSbuf[i]=0;
        context.state.AddXDS_c = 0;
        context.state.startXDS = 1;
        return;
    }
    context.state.AddXDS_XDSbuf[context.state.AddXDS_c++] = hi;
    context.state.AddXDS_XDSbuf[context.state.AddXDS_c++] = lo;
    if (end_packet)
    {
        context.state.startXDS = 1;
        j = 0;
        for (i = 0; i < context.state.AddXDS_c; i++)
            j += context.state.AddXDS_XDSbuf[i];
        if (context.state.AddXDS_c < 4 || (j & 0x7f) != 0)
        {
            context.state.AddXDS_c = 0;
            return;
        }
        context.state.lastXDS = std::clamp(context.state.lastXDS, 0, static_cast<int>(std::size(context.state.XDSbuffer)));
        for (i = 0; i < context.state.lastXDS; i++)
        {
            if (context.state.XDSbuffer[i][0] == context.state.AddXDS_XDSbuf[0] && context.state.XDSbuffer[i][1] == context.state.AddXDS_XDSbuf[1])
            {
                j = 0;
                while (j < std::min(context.state.AddXDS_c, 100))
                {
                    if (context.state.XDSbuffer[i][j] != context.state.AddXDS_XDSbuf[j])
                    {
                        while (j < 100)
                        {
                            context.state.XDSbuffer[i][j] = context.state.AddXDS_XDSbuf[j];
                            j++;
                        }
                        newXDS = 1;
                        break;
                    }
                    j++;
                }
                break;
            }
        }
        if (i == context.state.lastXDS)
        {
            i = std::min(i, static_cast<int>(std::size(context.state.XDSbuffer)) - 1);
            j = 0;
            while (j < 100)
            {
                context.state.XDSbuffer[i][j] = context.state.AddXDS_XDSbuf[j];
                j++;
            }
            newXDS = 1;
            context.state.lastXDS = std::min(context.state.lastXDS + 1, static_cast<int>(std::size(context.state.XDSbuffer)));
        }
        // The legacy duplicate cache stores only a 100-byte prefix. Longer
        // packets must still be interpreted when metadata changes in the tail.
        if (context.state.AddXDS_c > 100) newXDS = 1;
        context.state.firstXDS = 0;
        if (newXDS)
        {
            const auto frame = std::format("{}", context.state.framenum);
            const auto xds_debug = [&](const char* key, auto&&... values) {
                Debug(context, 10, "%s", context.translator.format(key,
                    std::forward<decltype(values)>(values)...).c_str());
            };
            xds_debug("caption_xds_bytes", frame,
                std::format("{:2x} {:2x} {:2x} {:2x} {:2x} {:2x} {:2x} {:2x} {:2x} {:2x} {:2x}",
                    context.state.AddXDS_XDSbuf[0], context.state.AddXDS_XDSbuf[1], context.state.AddXDS_XDSbuf[2],
                    context.state.AddXDS_XDSbuf[3], context.state.AddXDS_XDSbuf[4], context.state.AddXDS_XDSbuf[5],
                    context.state.AddXDS_XDSbuf[6], context.state.AddXDS_XDSbuf[7], context.state.AddXDS_XDSbuf[8],
                    context.state.AddXDS_XDSbuf[9], context.state.AddXDS_XDSbuf[10]));

            context.state.AddXDS_XDSbuf[context.state.AddXDS_c-2] = 0;
            for (i=2; i < context.state.AddXDS_c-2; i++)
                context.state.AddXDS_XDSbuf[i] &= 0x7f;

            if ((context.state.AddXDS_XDSbuf[0] & 0x7f) == 1)
            {
                const auto type = context.state.AddXDS_XDSbuf[1] & 0x7f;
                const int payload_length = context.state.AddXDS_c - 4;
                if ((type == 1 && payload_length < 4) ||
                    (type == 2 && payload_length < 2) ||
                    (type == 5 && payload_length < 2)) {
                    context.state.AddXDS_c = 0;
                    return;
                }
                if (type == 0x01)
                {
                    xds_debug("caption_xds_program_start", frame,
                        std::format("{:02}", context.state.AddXDS_XDSbuf[3] & 0x3f),
                        std::format("{:02}", context.state.AddXDS_XDSbuf[2] & 0x3f),
                        std::format("{}", context.state.AddXDS_XDSbuf[5] & 0x1f),
                        std::format("{}", context.state.AddXDS_XDSbuf[4] & 0x0f));
                }
                else if (type == 0x02)
                {
//					Debug(10, "XDS[%i]: Program Length\n", XDSbuf[2] & 0x38, XDSbuf[2] & 0x4f ,  XDSbuf[3] & 0x4f,  XDSbuf[3] & 0xb0);
                    xds_debug("caption_xds_program_length", frame,
                        std::format("{}", context.state.AddXDS_XDSbuf[3] & 0x3f),
                        std::format("{}", context.state.AddXDS_XDSbuf[2] & 0x3f),
                        std::format("{}", context.state.AddXDS_XDSbuf[5] & 0x3f),
                        std::format("{}", context.state.AddXDS_XDSbuf[4] & 0x3f),
                        std::format("{}", context.state.AddXDS_XDSbuf[6] & 0x3f));
                    if ( (context.state.AddXDS_XDSbuf[2] << 8) + context.state.AddXDS_XDSbuf[3] != context.state.XDS_block[context.state.XDS_block_count].duration)
                    {
                        Add_XDS_block(context);
                        context.state.XDS_block[context.state.XDS_block_count].duration = (context.state.AddXDS_XDSbuf[3] << 8) + context.state.AddXDS_XDSbuf[2];
                    }
                    if (payload_length >= 4 && (context.state.AddXDS_XDSbuf[4] << 8) + context.state.AddXDS_XDSbuf[5] != context.state.XDS_block[context.state.XDS_block_count].position)
                    {
                        Add_XDS_block(context);
                        context.state.XDS_block[context.state.XDS_block_count].position = (context.state.AddXDS_XDSbuf[5] << 8) + context.state.AddXDS_XDSbuf[4];
                    }


                    /*
                    01155         uint length_min  = xds_buf[2] & 0x3f;
                    01156         uint length_hour = xds_buf[3] & 0x3f;
                    01157         uint length_elapsed_min  = 0;
                    01158         uint length_elapsed_hour = 0;
                    01159         uint length_elapsed_secs = 0;
                    01160         if (xds_buf.size() > 6)
                    01161         {
                    01162             length_elapsed_min  = xds_buf[4] & 0x3f;
                    01163             length_elapsed_hour = xds_buf[5] & 0x3f;
                    01164         }
                    01165         if (xds_buf.size() > 8 && xds_buf[7] == 0x40)
                    01166             length_elapsed_secs = xds_buf[6] & 0x3f;
                    01167
                    01168         QString msg = QString("Program Length %1:%2%3 "
                    01169                               "Time in Show %4:%5%6.%7%8")
                    01170             .arg(length_hour).arg(length_min / 10).arg(length_min % 10)
                    01171             .arg(length_elapsed_hour)
                    01172             .arg(length_elapsed_min / 10).arg(length_elapsed_min % 10)
                    01173             .arg(length_elapsed_secs / 10).arg(length_elapsed_secs % 10);
                    01174
                    */

                }
                else if (type == 0x03)
                {
                    size_t n = sizeof(context.state.XDS_block[context.state.XDS_block_count].name);
                    if (strncmp((const char*) context.state.XDS_block[context.state.XDS_block_count].name, (const char*)&context.state.AddXDS_XDSbuf[2], n) != 0)
                    {
                        Add_XDS_block(context);
                        strncpy(context.state.XDS_block[context.state.XDS_block_count].name, (const char*) &context.state.AddXDS_XDSbuf[2], n - 1);
                        context.state.XDS_block[context.state.XDS_block_count].name[n - 1] = '\0';
                    }
                    xds_debug("caption_xds_program_name", frame,
                        reinterpret_cast<const char*>(&context.state.AddXDS_XDSbuf[2]));
//		XDS_block[XDS_block_count].name[0] = 0;
                }
                else if (context.state.AddXDS_XDSbuf[1] == 0x04)
                {
                    xds_debug("caption_xds_program_type", frame,
                        std::format("{:02x}", context.state.AddXDS_XDSbuf[2]));
                }
                else if (type == 0x05)
                {
                    xds_debug("caption_xds_vchip", frame,
                        std::format("{:2x}", context.state.AddXDS_XDSbuf[2] & 0x38),
                        std::format("{:2x}", context.state.AddXDS_XDSbuf[2] & 0x4f),
                        std::format("{:2x}", context.state.AddXDS_XDSbuf[3] & 0x4f),
                        std::format("{:2x}", context.state.AddXDS_XDSbuf[3] & 0xb0));
                    if ( (context.state.AddXDS_XDSbuf[2] << 8) + context.state.AddXDS_XDSbuf[3] != context.state.XDS_block[context.state.XDS_block_count].v_chip)
                    {
                        Add_XDS_block(context);
                        context.state.XDS_block[context.state.XDS_block_count].v_chip = (context.state.AddXDS_XDSbuf[2] << 8) + context.state.AddXDS_XDSbuf[3];
                    }

//							XDS_block[XDS_block_count].v_chip = 0;

                }
                else if (context.state.AddXDS_XDSbuf[1] == 0x86)
                {
                    xds_debug("caption_xds_audio_streams", frame);
                }
                else if (context.state.AddXDS_XDSbuf[1] == 0x07)
                {
                    xds_debug("caption_xds_caption_stream", frame);
                }
                else if (context.state.AddXDS_XDSbuf[1] == 0x08)
                {
                    xds_debug("caption_xds_copy_management", frame);
                }
                else if (context.state.AddXDS_XDSbuf[1] == 0x89)
                {
                    xds_debug("caption_xds_aspect_ratio", frame);
                }
                else if (context.state.AddXDS_XDSbuf[1] == 0x8c)
                {
                    xds_debug("caption_xds_program_data", frame,
                        reinterpret_cast<const char*>(&context.state.AddXDS_XDSbuf[2]));
                }
                else if (context.state.AddXDS_XDSbuf[1] == 0x0d)
                {
                    xds_debug("caption_xds_misc_data", frame,
                        reinterpret_cast<const char*>(&context.state.AddXDS_XDSbuf[2]));
                }
                else if (context.state.AddXDS_XDSbuf[1] == 0x010)
                {
                    xds_debug("caption_xds_program_description", frame,
                        reinterpret_cast<const char*>(&context.state.AddXDS_XDSbuf[2]));
                }
                else
                    xds_debug("caption_xds_unknown", frame);

            }
            else if (context.state.AddXDS_XDSbuf[0] == 0x85)
            {
                if (context.state.AddXDS_XDSbuf[1] == 0x01)
                {
                    xds_debug("caption_xds_network_name", frame,
                        reinterpret_cast<const char*>(&context.state.AddXDS_XDSbuf[2]));
                }
                else if (context.state.AddXDS_XDSbuf[1] == 0x02)
                {
                    xds_debug("caption_xds_network_call_name", frame,
                        reinterpret_cast<const char*>(&context.state.AddXDS_XDSbuf[2]));
                }
                else
                    xds_debug("caption_xds_unknown", frame);
            }
            else if (context.state.AddXDS_XDSbuf[0] == 0x0d)
            {
                xds_debug("caption_xds_private_data", frame,
                    reinterpret_cast<const char*>(&context.state.AddXDS_XDSbuf[2]));
            }
            else
            {
                for (i=0; i < context.state.AddXDS_c - 2; i++)
                {
                    context.state.AddXDS_XDSbuf[i] &= 0x7f;
                    if (context.state.AddXDS_XDSbuf[i] < 0x20)
                        context.state.AddXDS_XDSbuf[i] = ' ';
                    else if (context.state.AddXDS_XDSbuf[i] == 0x20)
                        context.state.AddXDS_XDSbuf[i] = '_';
                }
                context.state.AddXDS_XDSbuf[context.state.AddXDS_c - 2] = 0;
                xds_debug("caption_xds_text", frame,
                    reinterpret_cast<const char*>(context.state.AddXDS_XDSbuf));
            }
        }
        for (i = 0; i < 256; i++)
            context.state.AddXDS_XDSbuf[i]=0;
        context.state.AddXDS_c = 0;
    }
}

void AddCC(RecordingContext& context, int i)
{
    bool			tempBool;
    long			current_frame = context.state.framenum;
    int hi,lo;

    unsigned char	charmap[0x60] =
    {
        ' ',
        '!',
        '"',
        '#',
        '$',
        '%',
        '&',
        '\'',
        '(',
        ')',
        0xe1,
        '+',
        ',',
        '-',
        '.',
        '/',
        '0',
        '1',
        '2',
        '3',
        '4',
        '5',
        '6',
        '7',
        '8',
        '9',
        ':',
        ';',
        '<',
        '=',
        '>',
        '?',
        '@',
        'A',
        'B',
        'C',
        'D',
        'E',
        'F',
        'G',
        'H',
        'I',
        'J',
        'K',
        'L',
        'M',
        'N',
        'O',
        'P',
        'Q',
        'R',
        'S',
        'T',
        'U',
        'V',
        'W',
        'X',
        'Y',
        'Z',
        '[',
        0xe9,
        ']',
        0xed,
        0xf3,
        0xfa,
        'a',
        'b',
        'c',
        'd',
        'e',
        'f',
        'g',
        'h',
        'i',
        'j',
        'k',
        'l',
        'm',
        'n',
        'o',
        'p',
        'q',
        'r',
        's',
        't',
        'u',
        'v',
        'w',
        'x',
        'y',
        'z',
        0xe7,
        0xf7,
        'N',
        'n',
        '?'
    };
    context.state.cc.cc1[0] &= 0x7f;
    context.state.cc.cc1[1] &= 0x7f;
    if (context.state.cc.cc1[0] == 0 && context.state.cc.cc1[1] == 0)
        return;


    current_frame++;
    /*
        if ((cc.cc1[0] != 0x14 && cc.cc1[0] < 0x20)) {
            cc.cc1[0] = ' ';
            cc.cc1[1] = 0;
        }
    */


    hi = context.state.cc.cc1[0];
    lo = context.state.cc.cc1[1];


//	if (hi == ' ' && lo == 'B')
//		hi = hi;


    if (hi>=0x18 && hi<=0x1f)
        hi=hi-8;
    switch (hi)
    {
    case 0x10:
//      if (lo>=0x40 && lo<=0x5f)
//          handle_pac (hi,lo,wb);
        break;
    case 0x11:
        if (lo>=0x20 && lo<=0x2f)
        {
            context.state.cc.cc1[0] = 0x20;
            context.state.cc.cc1[1] = 0x00;
        }
//          handle_text_attr (hi,lo,wb);
        if (lo>=0x30 && lo<=0x3f)
        {
            context.state.cc.cc1[0] = 0x20;
            context.state.cc.cc1[1] = 0x00;
//	wrote_to_screen=1;
//          handle_double (hi,lo,wb);
        }
        if (lo>=0x40 && lo<=0x7f)
        {
//          handle_pac (hi,lo,wb);
            context.state.cc.cc1[0] = 0x20;
            context.state.cc.cc1[1] = 0x00;
        }
        break;
    case 0x12:
    case 0x13:
        if (lo>=0x20 && lo<=0x3f)
        {
            context.state.cc.cc1[0] = 0x20;
            context.state.cc.cc1[1] = 0x00;
//          handle_extended (hi,lo,wb);
//			wrote_to_screen=1;
        }
//        if (lo>=0x40 && lo<=0x7f)
//          handle_pac (hi,lo,wb);
        break;
    case 0x14:
    case 0x15:
//        if (lo>=0x20 && lo<=0x2f)
//          handle_command (hi,lo,wb);
//        if (lo>=0x40 && lo<=0x7f)
//          handle_pac (hi,lo,wb);
        break;
    case 0x16:
//        if (lo>=0x40 && lo<=0x7f)
//          handle_pac (hi,lo,wb);
        break;
    case 0x17:
//        if (lo>=0x21 && lo<=0x22)
//           handle_command (hi,lo,wb);
//        if (lo>=0x2e && lo<=0x2f)
//            handle_text_attr (hi,lo,wb);
//        if (lo>=0x40 && lo<=0x7f)
//            handle_pac (hi,lo,wb);
        break;
    }







    if ((context.state.cc.cc1[0] >= 0x20) && (context.state.cc.cc1[0] < 0x80))
    {
        if ((context.state.current_cc_type == ROLLUP) || (context.state.current_cc_type == PAINTON))
        {
            context.state.cc_on_screen = true;
        }
        else if (context.state.current_cc_type == POPON)
        {
            context.state.cc_in_memory = true;
        }

        Debug(context, 11, "%i:%i) %i:'%c':%x\t", context.state.cc_text_count, context.state.cc_text[context.state.cc_text_count].text_len, i, charmap[context.state.cc.cc1[0] - 0x20], context.state.cc.cc1[0]);
        context.state.cc_text[context.state.cc_text_count].text[context.state.cc_text[context.state.cc_text_count].text_len] = charmap[context.state.cc.cc1[0] - 0x20];
        context.state.cc_text[context.state.cc_text_count].text_len++;
        context.state.cc_text[context.state.cc_text_count].text[context.state.cc_text[context.state.cc_text_count].text_len] = '\0';
        if ((context.state.cc.cc1[1] >= 0x20) && (context.state.cc.cc1[1] < 0x80))
        {
            Debug(context, 11, "%i:%i) %i:'%c':%x\t", context.state.cc_text_count, context.state.cc_text[context.state.cc_text_count].text_len, i, charmap[context.state.cc.cc1[1] - 0x20], context.state.cc.cc1[1]);
            context.state.cc_text[context.state.cc_text_count].text[context.state.cc_text[context.state.cc_text_count].text_len] = charmap[context.state.cc.cc1[1] - 0x20];
            context.state.cc_text[context.state.cc_text_count].text_len++;
            context.state.cc_text[context.state.cc_text_count].text[context.state.cc_text[context.state.cc_text_count].text_len] = '\0';
            if ((context.state.last_cc_type == ROLLUP) || (context.state.last_cc_type == PAINTON))
            {
                context.state.cc_on_screen = true;
            }
            else if (context.state.last_cc_type == POPON)
            {
                context.state.cc_in_memory = true;
            }
        }
    }

    if (((context.state.cc_text[context.state.cc_text_count].text_len > 200) &&
         !isalpha(static_cast<unsigned char>(context.state.cc_text[context.state.cc_text_count].text[context.state.cc_text[context.state.cc_text_count].text_len - 1]))) ||
            (context.state.cc_text[context.state.cc_text_count].text_len > 245))
    {
        context.state.cc_text[context.state.cc_text_count].end_frame = current_frame - 1;
        context.state.cc_text_count++;
        InitializeCCTextArray(context, context.state.cc_text_count);
        context.state.cc_text[context.state.cc_text_count].start_frame = current_frame;
        context.state.cc_text[context.state.cc_text_count].text_len = 0;
    }

    if (context.state.cc.cc1[0] == 0x14)
    {
        if ((context.state.cc.cc1[0] == context.state.lastcc.cc1[0]) && (context.state.cc.cc1[1] == context.state.lastcc.cc1[1]))
        {
            CaptionDebug(context, 11, "caption_double_code");
            return;
        }

        switch (context.state.cc.cc1[1])
        {
        case 0x20:
            CaptionDebug(context, 11, "caption_control_resume_loading",
                std::format("{:6}", current_frame));
            context.state.cc_text[context.state.cc_text_count].end_frame = current_frame - 1;
            context.state.cc_text_count++;
            InitializeCCTextArray(context, context.state.cc_text_count);
            context.state.cc_text[context.state.cc_text_count].start_frame = current_frame;
            context.state.cc_text[context.state.cc_text_count].text_len = 0;
            context.state.last_cc_type = POPON;
            context.state.current_cc_type = POPON;
            AddNewCCBlock(context, current_frame, context.state.current_cc_type, context.state.cc_on_screen, context.state.cc_in_memory);
            break;

        case 0x21:
            // Debug(11, "Frame - %6i Control Code
            // Found:\tBackSpace\n", current_frame);
            break;

        case 0x22:
            // Debug(11, "Frame - %6i Control Code
            // Found:\tAlarm Off\n", current_frame);
            break;

        case 0x23:
            // Debug(11, "Frame - %6i Control Code
            // Found:\tAlarm On\n", current_frame);
            break;

        case 0x24:
            // Debug(11, "Frame - %6i Control Code
            // Found:\tDelete to end of row\n", current_frame);
            break;

        case 0x25:
            CaptionDebug(context, 11, "caption_control_rollup_2",
                std::format("{:6}", current_frame));
            context.state.cc_text[context.state.cc_text_count].end_frame = current_frame - 1;
            context.state.cc_text_count++;
            InitializeCCTextArray(context, context.state.cc_text_count);
            context.state.cc_text[context.state.cc_text_count].start_frame = current_frame;
            context.state.cc_text[context.state.cc_text_count].text_len = 0;
            context.state.last_cc_type = ROLLUP;
            context.state.current_cc_type = ROLLUP;
            AddNewCCBlock(context, current_frame, context.state.current_cc_type, context.state.cc_on_screen, context.state.cc_in_memory);
            break;

        case 0x26:
            CaptionDebug(context, 11, "caption_control_rollup_3",
                std::format("{:6}", current_frame));
            context.state.cc_text[context.state.cc_text_count].end_frame = current_frame - 1;
            context.state.cc_text_count++;
            InitializeCCTextArray(context, context.state.cc_text_count);
            context.state.cc_text[context.state.cc_text_count].start_frame = current_frame;
            context.state.cc_text[context.state.cc_text_count].text_len = 0;
            context.state.last_cc_type = ROLLUP;
            context.state.current_cc_type = ROLLUP;
            AddNewCCBlock(context, current_frame, context.state.current_cc_type, context.state.cc_on_screen, context.state.cc_in_memory);
            break;

        case 0x27:
            CaptionDebug(context, 11, "caption_control_rollup_4",
                std::format("{:6}", current_frame));
            context.state.cc_text[context.state.cc_text_count].end_frame = current_frame - 1;
            context.state.cc_text_count++;
            InitializeCCTextArray(context, context.state.cc_text_count);
            context.state.cc_text[context.state.cc_text_count].start_frame = current_frame;
            context.state.cc_text[context.state.cc_text_count].text_len = 0;
            context.state.last_cc_type = ROLLUP;
            context.state.current_cc_type = ROLLUP;
            AddNewCCBlock(context, current_frame, context.state.current_cc_type, context.state.cc_on_screen, context.state.cc_in_memory);
            break;

        case 0x28:
            // Debug(11, "Frame - %6i Control Code
            // Found:\tFlash On\n", current_frame);
            break;

        case 0x29:
            CaptionDebug(context, 11, "caption_control_resume_direct",
                std::format("{:6}", current_frame));
            context.state.cc_text[context.state.cc_text_count].end_frame = current_frame - 1;
            context.state.cc_text_count++;
            InitializeCCTextArray(context, context.state.cc_text_count);
            context.state.cc_text[context.state.cc_text_count].start_frame = current_frame;
            context.state.cc_text[context.state.cc_text_count].text_len = 0;
            context.state.last_cc_type = PAINTON;
            context.state.current_cc_type = PAINTON;
            AddNewCCBlock(context, current_frame, context.state.current_cc_type, context.state.cc_on_screen, context.state.cc_in_memory);
            break;

        case 0x2A:
            // Debug(11, "Frame - %6i Control Code
            // Found:\tText Restart\n", current_frame);
            break;

        case 0x2B:
            // Debug(11, "Frame - %6i Control Code
            // Found:\tResume Text Display\n", current_frame);
            break;

        case 0x2C:
            CaptionDebug(context, 11, "caption_control_erase_displayed",
                std::format("{:6}", current_frame));
            context.state.cc_text[context.state.cc_text_count].end_frame = current_frame - 1;
            context.state.cc_text_count++;
            InitializeCCTextArray(context, context.state.cc_text_count);
            context.state.cc_text[context.state.cc_text_count].start_frame = current_frame;
            context.state.cc_text[context.state.cc_text_count].text_len = 0;
            context.state.cc_on_screen = false;
            context.state.current_cc_type = NONE;
            AddNewCCBlock(context, current_frame, context.state.current_cc_type, context.state.cc_on_screen, context.state.cc_in_memory);
            break;

        case 0x2D:
            // Debug(11, "Frame - %6i Control Code
            // Found:\tCarriage Return\n", current_frame);
            if (context.state.cc_text[context.state.cc_text_count].text_len > 200)
            {
                context.state.cc_text[context.state.cc_text_count].end_frame = current_frame - 1;
                context.state.cc_text_count++;
                InitializeCCTextArray(context, context.state.cc_text_count);
                context.state.cc_text[context.state.cc_text_count].start_frame = current_frame;
                context.state.cc_text[context.state.cc_text_count].text_len = 0;
            }

            context.state.cc_text[context.state.cc_text_count].text[context.state.cc_text[context.state.cc_text_count].text_len] = ' ';
            context.state.cc_text[context.state.cc_text_count].text_len++;
            context.state.cc_text[context.state.cc_text_count].text[context.state.cc_text[context.state.cc_text_count].text_len] = '\0';
            Debug(context, 11, "\n");
            break;

        case 0x2E:
            CaptionDebug(context, 11, "caption_control_erase_nondisplayed",
                std::format("{:6}", current_frame));

            // cc_text_count++;
            // InitializeCCTextArray(cc_text_count);
            context.state.cc_in_memory = false;
            break;

        case 0x2F:
            CaptionDebug(context, 11, "caption_control_end",
                std::format("{:6}", current_frame),
                std::format("{}", static_cast<int>(context.state.cc_in_memory)),
                std::format("{}", static_cast<int>(context.state.cc_on_screen)));
            context.state.cc_text[context.state.cc_text_count].end_frame = current_frame - 1;
            context.state.cc_text_count++;
            InitializeCCTextArray(context, context.state.cc_text_count);
            context.state.cc_text[context.state.cc_text_count].start_frame = current_frame;
            context.state.cc_text[context.state.cc_text_count].text_len = 0;
            tempBool = context.state.cc_in_memory;
            context.state.cc_in_memory = context.state.cc_on_screen;
            context.state.cc_on_screen = tempBool;
            if (!context.state.cc_on_screen)
            {
                context.state.current_cc_type = NONE;
            }
            else
            {
                if ((context.state.cc_block_count > 0) && (context.state.cc_block[context.state.cc_block_count].type == NONE))
                {
                    context.state.current_cc_type = context.state.last_cc_type;
                }
            }

            AddNewCCBlock(context, current_frame, context.state.current_cc_type, context.state.cc_on_screen, context.state.cc_in_memory);
            break;

        default:
            CaptionDebug(context, 11, "caption_control_unknown",
                std::format("{:6}", current_frame), std::format("{:2X}", context.state.cc.cc1[1]));
            if (context.state.cc_text[context.state.cc_text_count].text_len > 200)
            {
                context.state.cc_text[context.state.cc_text_count].end_frame = current_frame - 1;
                context.state.cc_text_count++;
                InitializeCCTextArray(context, context.state.cc_text_count);
                context.state.cc_text[context.state.cc_text_count].start_frame = current_frame;
                context.state.cc_text[context.state.cc_text_count].text_len = 0;
            }

            context.state.cc_text[context.state.cc_text_count].text[context.state.cc_text[context.state.cc_text_count].text_len] = ' ';
            context.state.cc_text[context.state.cc_text_count].text_len++;
            context.state.cc_text[context.state.cc_text_count].text[context.state.cc_text[context.state.cc_text_count].text_len] = '\0';
            break;
        }
    }

    context.state.lastcc.cc1[0] = context.state.cc.cc1[0];
    context.state.lastcc.cc1[1] = context.state.cc.cc1[1];

}

void ProcessCCData(RecordingContext& context)
{
    int				i;
    int proceed = 0;
    int is_CC = 0;
    //int is_dish = 0;
    int is_GA = 0;
    int cctype = 0;
    int offset;
    char temp[2000];
    char hex[10];
    unsigned char t;
    unsigned char *p;
    bool			cc1First = false;
    unsigned char	packetCount;

    if (!context.state.initialized) return;
    // Validate the complete packet before changing screen or deferred-pair
    // state. A padded backing array is not evidence that bytes were received.
    const int length = context.state.ccDataLen;
    const auto* data = context.state.ccData;
    if (length < 2 || length > static_cast<int>(std::size(context.state.ccData))) return;
    if (data[0] == 'C' && data[1] == 'C') {
        if (length < 5 || data[2] != 1 || data[3] != 0xf8 ||
            length < 5 + ((data[4] & 0x1e) / 2) * 6) return;
    } else if (data[0] == 'G' && data[1] == 'A') {
        if (length < 7 || data[2] != '9' || data[3] != '4' || data[4] != 3 ||
            length < 7 + (data[5] & 0x1f) * 3) return;
    } else if (data[0] == 5 && data[1] == 2) {
        if (length < 8) return;
        const int type = data[7];
        if (type == 2) {
            if (length < 13 || (data[11] == 4 && (data[12] & 0x7f) < 32 && length < 14)) return;
        } else if (type == 4) {
            if (length < 13) return;
        } else if (type == 5) {
            if (length < 18 || context.state.prevccDataLen < 0 ||
                context.state.prevccDataLen > static_cast<int>(std::size(context.state.prevccData)) ||
                context.state.prevccDataLen % 2 != 0) return;
            if (data[14] == 2) {
                if (length < 20 || (data[18] == 4 && (data[19] & 0x7f) < 32 && length < 21)) return;
            } else if (length < 20) return;
        }
    } else return;

    // Reset state on the first frame
    if (context.state.framenum == 0) {
        context.state.last_cc_type = NONE;
        context.state.current_cc_type = NONE;
        context.state.cc_on_screen = false;
        context.state.cc_in_memory = false;
    }

    if (context.settings.verbose >= 12)
    {
        p = (unsigned char *)temp;
        for (i = 0; i < context.state.ccDataLen; i++)
        {
            t = context.state.ccData[i] & 0x7f;
            if (t == 0x20)
                *p++ = '_';
            else if (t > 0x20 && t < 0x7f)
                *p++ = t;
            else
                *p++ = ' ';
            *p++ = ' ';
            *p++ = ' ';
        }
        *p++ = 0;
        if (context.state.ccData[0] == 'G' && length > 7)
            temp[7*3] = '0' + (temp[7*3] & 0x03);
        CaptionDebug(context, 10, "caption_cc_data",
            std::format("{:4}", context.state.framenum), std::format("{}", context.state.pict_type),
            std::format("{:4}", context.state.ccDataLen), reinterpret_cast<const char*>(temp));

        p = (unsigned char *)temp;
        for (i = 0; i < context.state.ccDataLen; i++)
        {
            sprintf(hex, "%2x ",context.state.ccData[i]);
            *p++ = hex[0];
            *p++ = hex[1];
            *p++ = ' ';
        }
        *p++ = 0;
        CaptionDebug(context, 10, "caption_cc_data",
            std::format("{:4}", context.state.framenum), std::format("{}", context.state.pict_type),
            std::format("{:4}", context.state.ccDataLen), reinterpret_cast<const char*>(temp));

    }

    if ((char)context.state.ccData[0] == 'C' && (char)context.state.ccData[1] == 'C' && context.state.ccData[2] == 0x01 && context.state.ccData[3] == 0xf8)
    {
        context.state.reorderCC = 0;
        packetCount = context.state.ccData[4];
        if (packetCount & 0x80)
        {
            cc1First = true;
        }
        else
        {
            cc1First = false;
        }
        offset = 5;
        packetCount = (packetCount & 0x1E) / 2;
        if ((!cc1First) || (packetCount != 15))
        {
            CaptionDebug(context, 11, "caption_field_order",
                std::format("{}", static_cast<int>(cc1First)), std::format("{}", packetCount));
        }
        proceed = 1;
        is_CC = 1;
    }
    else 	if ((char)context.state.ccData[0] == 'G' && (char)context.state.ccData[1] == 'A' && context.state.ccData[2] == '9' && context.state.ccData[3] == '4'&& context.state.ccData[4] == 0x03)
    {
        context.state.reorderCC = 1;
        packetCount = context.state.ccData[5] & 0x1F;
        proceed = ( context.state.ccData[5] & 0x40) >> 6;
        offset = 7;
        is_GA = 1;
    }
    else 	if (context.state.ccData[0] == 0x05 && context.state.ccData[1] == 0x02)
    {
        context.state.reorderCC = 0;
        proceed = 0;
        offset = 7;
        cctype = context.state.ccData[offset++];
        if (cctype == 2)
        {
            offset++;
            context.state.cc.cc1[0] = context.state.ccData[offset++];
            context.state.cc.cc1[1] = context.state.ccData[offset++];
            AddCC(context, 1);
            cctype = context.state.ccData[offset++];
            if (cctype == 4 && ( context.state.ccData[offset] & 0x7f) < 32)
            {
                context.state.cc.cc1[0] = context.state.ccData[offset++];
                context.state.cc.cc1[1] = context.state.ccData[offset++];
                AddCC(context, 1);
            }
            offset += 3;
        }
        else if (cctype == 4)
        {
            offset++;
            context.state.cc.cc1[0] = context.state.ccData[offset++];
            context.state.cc.cc1[1] = context.state.ccData[offset++];
            AddCC(context, 1);
            context.state.cc.cc1[0] = context.state.ccData[offset++];
            context.state.cc.cc1[1] = context.state.ccData[offset++];
            AddCC(context, 1);
            offset += 3;
        }
        else if (cctype == 5)
        {
            for (i = 0; i < context.state.prevccDataLen; i +=2)
            {
                context.state.cc.cc1[0] = context.state.prevccData[i];
                context.state.cc.cc1[1] = context.state.prevccData[i+1];
                AddCC(context, i/2);
            }
            context.state.prevccDataLen = 0;
//			offset += 6;
            cctype = context.state.ccData[offset++] & 0x7f;
            cctype = context.state.ccData[offset++] & 0x7f;
            cctype = context.state.ccData[offset++] & 0x7f;
            cctype = context.state.ccData[offset++] & 0x7f;
            cctype = context.state.ccData[offset++] & 0x7f;
            cctype = context.state.ccData[offset++] & 0x7f;
//
            cctype = context.state.ccData[offset++];
            offset++;
            context.state.prevccDataLen = 0;
            context.state.prevccData[context.state.prevccDataLen++] = context.state.ccData[offset++];
            context.state.prevccData[context.state.prevccDataLen++] = context.state.ccData[offset++];
            if (cctype == 2)
            {
                cctype = context.state.ccData[offset++];
                if (cctype == 4 && ( context.state.ccData[offset] & 0x7f) < 32)
                {
                    context.state.prevccData[context.state.prevccDataLen++] = context.state.ccData[offset++];
                    context.state.prevccData[context.state.prevccDataLen++] = context.state.ccData[offset++];
                }
            }
            else
            {
                context.state.prevccData[context.state.prevccDataLen++] = context.state.ccData[offset++];
                context.state.prevccData[context.state.prevccDataLen++] = context.state.ccData[offset++];
            }
            offset += 3;
        }
        packetCount = cctype / 2;
        //is_dish = 1;
    }

    if (proceed)
    {

        for (i = 0; i < packetCount; i++)
        {
            if (is_CC)
            {
                if (cc1First)
                {
                    context.state.cc.cc1[0] = CheckOddParity(context.state.ccData[(i * 6) + offset + 1]) ? context.state.ccData[(i * 6) + offset + 1] & 0x7f : 0x00;
                    context.state.cc.cc1[1] = CheckOddParity(context.state.ccData[(i * 6) + offset + 2]) ? context.state.ccData[(i * 6) + offset + 2] & 0x7f : 0x00;
                }
                else
                {
                    context.state.cc.cc1[0] = CheckOddParity(context.state.ccData[(i * 6) + offset + 4]) ? context.state.ccData[(i * 6) + offset + 4] & 0x7f : 0x00;
                    context.state.cc.cc1[1] = CheckOddParity(context.state.ccData[(i * 6) + offset + 5]) ? context.state.ccData[(i * 6) + offset + 5] & 0x7f : 0x00;
                }
                AddCC(context, i);
            }
            if (is_GA)
            {

                if ((context.state.ccData[(i * 3) + offset] & 4) == 0)
                    continue;
                if (context.state.ccData[(i * 3) + offset] == 0xfa)
                    continue;
                if (context.state.ccData[(i * 3) + offset + 1]  == 0x80 && context.state.ccData[(i * 3) + offset + 2] == 0x80)
                    continue;
                if (context.state.ccData[(i * 3) + offset + 1]  == 0x00 && context.state.ccData[(i * 3) + offset + 2] == 0x00)
                    continue;

                cctype = (context.state.ccData[(i * 3) + offset] & 3);
//				cc.cc1[0] = CheckOddParity(ccData[(i * 3) + offset + 1]) ? ccData[(i * 3) + offset + 1] & 0x7f : 0x00;
//				cc.cc1[1] = CheckOddParity(ccData[(i * 3) + offset + 2]) ? ccData[(i * 3) + offset + 2] & 0x7f : 0x00;
                context.state.cc.cc1[0] = context.state.ccData[(i * 3) + offset + 1] & 0x7f;
                context.state.cc.cc1[1] = context.state.ccData[(i * 3) + offset + 2] & 0x7f;

                /*
                if (cctype == 0)
                    cctype = cctype;
                */
                if (cctype == 1)
                    AddXDS(context, context.state.ccData[(i * 3) + offset + 1], context.state.ccData[(i * 3) + offset + 2]);
                /*
                if (cctype == 2)
                    cctype = cctype;
                if (cctype == 3)
                    cctype = cctype;
                */
                if (cctype != 0 && cctype != 1 )
                    continue;
                if ( cctype == 0 /* || cctype == 1 */ )
                {
//					cc.cc1[0] = ccData[(i * 3) + offset + 1] & 0x7f;
//					cc.cc1[1] = ccData[(i * 3) + offset + 2] & 0x7f;
                    AddCC(context, i);

                }
                else
                {
                    context.state.cc.cc1[0] = 0;
                    context.state.cc.cc1[1] = 0;
                }
            }
            /*
                        if (is_dish) {

                            if (cctype == 2 || cctype == 4) {
                                cc.cc1[0] = ccData[(i * 3) + offset + 1] & 0x7f;
                                cc.cc1[1] = ccData[(i * 3) + offset + 2] & 0x7f;
                                offset = offset - 1;
                                AddCC(i);

                            } else
                                continue;

                        }
            */
        }
    }
}

bool CheckOddParity(unsigned char ch)
{
    int				i;
    unsigned char	test = 1;
    int				count = 0;
    for (i = 1; i <= 8; i++)
    {
        if (ch & test) count++;
        test *= 2;
    }

    if (count % 2)
    {
        return (true);
    }
    else
    {
        return (false);
    }
}

void AddNewCCBlock(RecordingContext& context, long current_frame, int type, bool cc_on_screen, bool cc_in_memory)
{
    if (context.state.cc_block[context.state.cc_block_count].type == type)
    {
        context.state.cc_block[context.state.cc_block_count].end_frame = current_frame;
    }
    else
    {
        CaptionDebug(context, 11, "caption_type_start", std::format("{:6}", current_frame),
            CaptionTypeText(context, type));
        if (context.state.cc_block[context.state.cc_block_count].end_frame == -1)
        {
            CaptionDebug(context, 11, "caption_new_block");
            context.state.cc_block[context.state.cc_block_count].end_frame = current_frame - 1;
            context.state.cc_block_count++;
            InitializeCCBlockArray(context, context.state.cc_block_count);
            context.state.cc_block[context.state.cc_block_count].start_frame = current_frame;
            context.state.cc_block[context.state.cc_block_count].type = type;
            if (context.state.cc_block_count > 1)
            {
                if ((F2L(context.state.cc_block[context.state.cc_block_count - 1].end_frame, context.state.cc_block[context.state.cc_block_count - 1].start_frame) < 1.0) &&
                        (context.state.cc_block[context.state.cc_block_count].type == context.state.cc_block[context.state.cc_block_count - 2].type) &&
                        (context.state.cc_block[context.state.cc_block_count].type != NONE))
                {
                    context.state.cc_block_count -= 2;
                    context.state.cc_block[context.state.cc_block_count].end_frame = -1;
                }
            }
        }
        else
        {
            context.state.cc_block_count++;
            InitializeCCBlockArray(context, context.state.cc_block_count);
            context.state.cc_block[context.state.cc_block_count].start_frame = current_frame;
            context.state.cc_block[context.state.cc_block_count].type = type;
        }

        OutputCCBlock(context, context.state.cc_block_count);
    }
}

char* CCTypeToStr(RecordingContext& context, int type)
{
    if (context.state.processCC)
    {
        switch (type)
        {
        case NONE:
            sprintf(context.state.tempString, "NONE");
            break;

        case ROLLUP:
            sprintf(context.state.tempString, "ROLLUP");
            break;

        case PAINTON:
            sprintf(context.state.tempString, "PAINTON");
            break;

        case POPON:
            sprintf(context.state.tempString, "POPON");
            break;

        case COMMERCIAL:
            sprintf(context.state.tempString, "COMMERCIAL");
            break;

        default:
            sprintf(context.state.tempString, "%d",type);
            break;
        }
    }
    else
    {
        context.state.tempString[0]=0; // was: sprintf(tempString, "");
    }

    return (context.state.tempString);
}

int DetermineCCTypeForBlock(RecordingContext& context, long start, long end)
{
    int type = NONE;
    int i = 0;
    int j = 0;
    int cc_block_first = context.state.cc_block_count;
    int cc_block_last = 0;
    int cc_type_count[5] = { 0, 0, 0, 0, 0 };
    while (context.state.cc_block[cc_block_first].start_frame > start) cc_block_first--;
    while (context.state.cc_block[cc_block_last].end_frame < end) cc_block_last++;

    // Look for the PAINTON then POPON pattern that is common in commercials
    for (i = cc_block_first; i <= cc_block_last; i++)
    {
        if (context.state.cc_block[i].type != NONE)
        {
            if (i > 0)
            {
                if ((context.state.cc_block[i - 1].type == PAINTON) && (context.state.cc_block[i].type == POPON))
                {
 //                   type = COMMERCIAL;
                    break;
                }
            }

            if (i > 1)
            {
                if ((context.state.cc_block[i - 2].type == PAINTON) &&
                        (context.state.cc_block[i - 1].type == NONE) &&
                        (F2L(context.state.cc_block[i - 1].end_frame, context.state.cc_block[i - 1].start_frame) <= 1.5) &&
                        (context.state.cc_block[i].type == POPON))
                {
 //                   type = COMMERCIAL;
                    break;
                }
            }
        }
    }

    // If no commercial pattern found, find the most common type of CC
    if (type != COMMERCIAL)
    {
        for (i = start; i <= end; i++)
        {
            for (j = 0; j < context.state.cc_block_count; j++)
            {
                if ((i > context.state.cc_block[j].start_frame) && (i < context.state.cc_block[j].end_frame))
                {
                    cc_type_count[context.state.cc_block[j].type]++;
                    break;
                }
            }
        }

        type = 0;
        for (i = 0; i < 5; i++)
        {
            if (cc_type_count[i] > cc_type_count[type])
            {
                type = i;
            }
        }
    }

    CaptionDebug(context, 4, "caption_block_summary", std::format("{:6}", start),
        std::format("{:6}", end), std::format("{:2}", cc_block_first),
        std::format("{:2}", cc_block_last), CaptionTypeText(context, type));

    return (type);
}



void SetARofBlocks(RecordingContext& context)
{
    int		i, j,k;
    double	sumAR = 0.0;
    int		frameCount = 0;
    if (!(context.settings.commDetectMethod & AR))
        return;
    k = 0;
    for (i = 0; i < context.state.block_count; i++)
    {
        sumAR = 0.0;
        frameCount = 0; // To prevent divide by zero error
        for (j = context.state.cblock[i].f_start + context.state.cblock[i].b_head;
                j < context.state.cblock[i].f_end - (int) context.state.cblock[i].b_tail; j++)
        {
            if ( k < context.state.ar_block_count && j >= context.state.ar_block[k].end )
                k++;
            if (context.state.ar_block[k].ar_ratio > 1)
            {
                sumAR += context.state.ar_block[k].ar_ratio;
                frameCount++;
            }
        }
        if (frameCount == 0)
            context.state.cblock[i].ar_ratio = 1.0;
        else
            context.state.cblock[i].ar_ratio = sumAR / (frameCount);
    }
}



bool ProcessCCDict(RecordingContext& context)
{
    int		i, j;
    char*	ptr;
    char	phrase[1024];
    bool	goodPhrase = true;
    auto dict = comskip::platform::own_file(myfopen(context.state.dictfilename.c_str(), "r"));
    if (!dict)
    {
        return (false);
    }

    Debug(context, 2, "%s", context.translator.text("caption_dictionary_start"));
    while (fgets(phrase, sizeof(phrase), dict.get()) != NULL)
    {
        ptr = strchr(phrase, '\n');
        if (ptr != NULL) *ptr = '\0';
        if (strstr(phrase, "-----") != NULL)
        {
            goodPhrase = false;
            Debug(context, 3, "%s", context.translator.text("caption_dictionary_bad_phrases"));
            continue;
        }
        // just in case the line is empty
        if (strlen(phrase) < 1) continue;

        Debug(context, 3, "%s", context.translator.format("caption_dictionary_search", phrase).c_str());
        for (i = 0; i < context.state.cc_text_count; i++)
        {
            if (strstr(_strupr((char*)context.state.cc_text[i].text), _strupr((char*)phrase)) != NULL)
            {
                Debug(context, 2, "%s", context.translator.format("caption_dictionary_found", phrase,
                    std::format("{}", i)).c_str());
                if (goodPhrase)
                {
                    j = FindBlock(context, (context.state.cc_text[i].start_frame + context.state.cc_text[i].end_frame) / 2);
                    if (j == -1)
                    {
                        Debug(context, 1, "%s", context.translator.format("caption_dictionary_block_error",
                            std::format("{}", i)).c_str());
                    }
                    else
                    {
                        Debug(context, 3, "%s", context.translator.format("scoring_score_before",
                            std::format("{}", j), std::format("{:.2f}", context.state.cblock[j].score)).c_str());
                        context.state.cblock[j].score /= context.state.dictionary_modifier;
                        Debug(context, 3, "%s", context.translator.format("scoring_score_after",
                            std::format("{:.2f}", context.state.cblock[j].score)).c_str());
                    }
                }
                else
                {
                    j = FindBlock(context, (context.state.cc_text[i].start_frame + context.state.cc_text[i].end_frame) / 2);
                    if (j == -1)
                    {
                        Debug(context, 1, "%s", context.translator.format("caption_dictionary_block_error",
                            std::format("{}", i)).c_str());
                    }
                    else
                    {
                        Debug(context, 3, "%s", context.translator.format("scoring_score_before",
                            std::format("{}", j), std::format("{:.2f}", context.state.cblock[j].score)).c_str());
                        context.state.cblock[j].score *= context.state.dictionary_modifier;
                        Debug(context, 3, "%s", context.translator.format("scoring_score_after",
                            std::format("{:.2f}", context.state.cblock[j].score)).c_str());
                    }
                }
            }
        }
    }

    return (true);
}
