#include "legacy_detection.h"

void OutputCCBlock(long i)
{
    if (i > 1)
    {
        Debug(
            11,
            "%i\tStart - %6i\tEnd - %6i\tType - %s\n",
            i - 2,
            cc_block[i - 2].start_frame,
            cc_block[i - 2].end_frame,
            CCTypeToStr(cc_block[i - 2].type)
        );
    }

    if (i > 0)
    {
        Debug(
            11,
            "%i\tStart - %6i\tEnd - %6i\tType - %s\n",
            i - 1,
            cc_block[i - 1].start_frame,
            cc_block[i - 1].end_frame,
            CCTypeToStr(cc_block[i - 1].type)
        );
    }

    if (i <= 0)
    {
        Debug(
            11,
            "%i\tStart - %6i\tEnd - %6i\tType - %s\n",
            i,
            cc_block[i].start_frame,
            cc_block[i].end_frame,
            CCTypeToStr(cc_block[i - 1].type)
        );
    }
}

void Init_XDS_block()
{
    if(!XDS_block)
    {
        max_XDS_block_count = 2000;
        XDS_block = static_cast<XDS_block_info *>( malloc((max_XDS_block_count + 1) * sizeof(XDS_block_info)) );
        if (XDS_block == NULL)
        {
            Debug(0, "Could not allocate memory for XDS blocks\n");
            exit(22);
        }
        XDS_block_count = 0;
        XDS_block[XDS_block_count].frame = 0;
        XDS_block[XDS_block_count].name[0] = 0;
        XDS_block[XDS_block_count].v_chip = 0;
        XDS_block[XDS_block_count].duration = 0;
        XDS_block[XDS_block_count].position = 0;
        XDS_block[XDS_block_count].composite1 = 0;
        XDS_block[XDS_block_count].composite2 = 0;
    }
}

void Add_XDS_block()
{
    if (XDS_block_count < max_XDS_block_count)
    {
        XDS_block_count++;
        XDS_block[XDS_block_count] = XDS_block[XDS_block_count-1];
        XDS_block[XDS_block_count].frame = framenum;
        frame[framenum].xds = XDS_block_count;

    }
    else
        Debug(0, "Too much XDS data, discarded\n");
}


unsigned char XDSbuffer[40][100];
int lastXDS = 0;
int firstXDS = 1;
int startXDS = 1;
int baseXDS = 0;

const char *ratingSystem[4] = { "MPAA", "TPG", "CE", "CF" };

#define MAXXDSBUFFER	1024
void AddXDS(unsigned char hi, unsigned char lo)
{
    static unsigned char XDSbuf[MAXXDSBUFFER];
    static int c = 0;
    int i,j;
    int newXDS = 0;
    Init_XDS_block();
    if (startXDS)
    {
        if ((hi & 0x70) == 0 && hi != 0x8f)
        {
            startXDS = 0;
            c = 0;
            baseXDS = hi & 0x0f;;
        }
        else
            return;
    }
    else
    {
        if ((hi & 0x7f) == baseXDS + 1)
            return; // COntinueation code
        if ((hi & 0x70) == 0 && hi != 0x8f)
            return;
    }
    if ((hi & 0x01) == 0 && (hi & 0x70) == 0x00 && hi != 0x8f)
        return;
    if (hi == 0x86 && (lo == 0x02 || lo == 1))
        return;
    if (c >= MAXXDSBUFFER - 4)
    {
        for (i = 0; i < 256; i++)
            XDSbuf[i]=0;
        c = 0;
        startXDS = 1;
        return;
    }
    XDSbuf[c++] = hi;
    XDSbuf[c++] = lo;
    if (hi == 0x8f)
    {
        startXDS = 1;
        j = 0;
        for (i = 0; i < c; i++)
            j += XDSbuf[i];
        if ( (j & 0x7f) != 0)
        {
            c = 0;
            return;
        }
        for (i = 0; i < lastXDS; i++)
        {
            if (XDSbuffer[i][0] == XDSbuf[0] && XDSbuffer[i][1] == XDSbuf[1])
            {
                j = 0;
                while (j < c)
                {
                    if (XDSbuffer[i][j] != XDSbuf[j])
                    {
                        while (j < 100)
                        {
                            XDSbuffer[i][j] = XDSbuf[j];
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
        if (i == lastXDS && !firstXDS)
        {
            j = 0;
            while (j < 100)
            {
                XDSbuffer[i][j] = XDSbuf[j];
                j++;
            }
            newXDS = 1;
            lastXDS++;
            i++;
        }
        firstXDS = 0;
        if (newXDS)
        {
            Debug(10, "XDS[%i]: %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x %2x ", framenum, XDSbuf[0], XDSbuf[1], XDSbuf[2], XDSbuf[3], XDSbuf[4], XDSbuf[5], XDSbuf[6], XDSbuf[7], XDSbuf[8], XDSbuf[9], XDSbuf[10], XDSbuf[11]);

            XDSbuf[c-2] = 0;
            for (i=2; i < c-2; i++)
                XDSbuf[i] &= 0x7f;

            if (XDSbuf[0] == 1)
            {
                if (XDSbuf[1] == 0x01)
                {
                    Debug(10, "XDS[%i]: Program Start Time %02d:%02d %d/%d\n", framenum, XDSbuf[3] & 0x3f, XDSbuf[2] & 0x3f ,  XDSbuf[5] & 0x1f,  XDSbuf[4] & 0x0f);
                }
                else if (XDSbuf[1] == 0x02)
                {
//					Debug(10, "XDS[%i]: Program Length\n", XDSbuf[2] & 0x38, XDSbuf[2] & 0x4f ,  XDSbuf[3] & 0x4f,  XDSbuf[3] & 0xb0);
                    Debug(10, "XDS[%i]: Program length %d:%d, elapsed %d:%d:%d.%d\n", framenum, XDSbuf[3] & 0x3f, XDSbuf[2] & 0x3f,  XDSbuf[5] & 0x3f,  XDSbuf[4] & 0x3f ,  XDSbuf[6] & 0x3f);
                    if ( (XDSbuf[2] << 8) + XDSbuf[3] != XDS_block[XDS_block_count].duration)
                    {
                        Add_XDS_block();
                        XDS_block[XDS_block_count].duration = (XDSbuf[3] << 8) + XDSbuf[2];
                    }
                    if ( (XDSbuf[4] << 8) + XDSbuf[5] != XDS_block[XDS_block_count].position)
                    {
                        Add_XDS_block();
                        XDS_block[XDS_block_count].position = (XDSbuf[5] << 8) + XDSbuf[4];
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
                else if (XDSbuf[1] == 0x83)
                {
                    size_t n = sizeof(XDS_block[XDS_block_count].name);
                    if (strncmp((const char*) XDS_block[XDS_block_count].name, (const char*)&XDSbuf[2], n) != 0)
                    {
                        Add_XDS_block();
                        strncpy(XDS_block[XDS_block_count].name, (const char*) &XDSbuf[2], n);
                    }
                    Debug(10, "XDS[%i]: Program Name: %s\n", framenum, &XDSbuf[2]);
//		XDS_block[XDS_block_count].name[0] = 0;
                }
                else if (XDSbuf[1] == 0x04)
                {
                    Debug(10, "XDS[%i]: Program Type: %0x\n", framenum, &XDSbuf[2]);
                }
                else if (XDSbuf[1] == 0x85)
                {
                    Debug(10, "XDS[%i]: V-Chip: %2x %2x %2x %2x\n", framenum, XDSbuf[2] & 0x38, XDSbuf[2] & 0x4f ,  XDSbuf[3] & 0x4f,  XDSbuf[3] & 0xb0);
                    if ( (XDSbuf[2] << 8) + XDSbuf[3] != XDS_block[XDS_block_count].v_chip)
                    {
                        Add_XDS_block();
                        XDS_block[XDS_block_count].v_chip = (XDSbuf[2] << 8) + XDSbuf[3];
                    }

//							XDS_block[XDS_block_count].v_chip = 0;

                }
                else if (XDSbuf[1] == 0x86)
                {
                    Debug(10, "XDS[%i]: Audio Streams \n", framenum);
                }
                else if (XDSbuf[1] == 0x07)
                {
                    Debug(10, "XDS[%i]: Caption Stream\n", framenum);
                }
                else if (XDSbuf[1] == 0x08)
                {
                    Debug(10, "XDS[%i]: Copy Management\n", framenum);
                }
                else if (XDSbuf[1] == 0x89)
                {
                    Debug(10, "XDS[%i]: Aspect Ratio\n", framenum);
                }
                else if (XDSbuf[1] == 0x8c)
                {
                    Debug(10, "XDS[%i]: Program Data, Name: %s\n", framenum, &XDSbuf[2]);
                }
                else if (XDSbuf[1] == 0x0d)
                {
                    Debug(10, "XDS[%i]: Miscellaneous Data: %s\n", framenum, &XDSbuf[2]);
                }
                else if (XDSbuf[1] == 0x010)
                {
                    Debug(10, "XDS[%i]: Program Description: %s\n", framenum, &XDSbuf[2]);
                }
                else
                    Debug(10, "XDS[%i]: Unknown\n", framenum, &XDSbuf[2]);

            }
            else if (XDSbuf[0] == 0x85)
            {
                if (XDSbuf[1] == 0x01)
                {
                    Debug(10, "XDS[%i]: Network Name: %s\n", framenum, &XDSbuf[2]);
                }
                else if (XDSbuf[1] == 0x02)
                {
                    Debug(10, "XDS[%i]: Network Call Name: %s\n", framenum, &XDSbuf[2]);
                }
                else
                    Debug(10, "XDS[%i]: Unknown\n", framenum, &XDSbuf[2]);
            }
            else if (XDSbuf[0] == 0x0d)
            {
                Debug(10, "XDS[%i]: Private Data: %s\n", framenum, &XDSbuf[2]);
            }
            else
            {
                for (i=0; i < 256; i++)
                {
                    XDSbuf[i] &= 0x7f;
                    if (XDSbuf[i] < 0x20)
                        XDSbuf[i] = ' ';
                    else if (XDSbuf[i] == 0x20)
                        XDSbuf[i] = '_';
                }
                Debug(10, "XDS[%i]: %s\n", framenum, XDSbuf);
            }
        }
        for (i = 0; i < 256; i++)
            XDSbuf[i]=0;
        c = 0;
    }
}

void AddCC(int i)
{
    bool			tempBool;
    long			current_frame = framenum;
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
    cc.cc1[0] &= 0x7f;
    cc.cc1[1] &= 0x7f;
    if (cc.cc1[0] == 0 && cc.cc1[1] == 0)
        return;


    current_frame++;
    /*
    	if ((cc.cc1[0] != 0x14 && cc.cc1[0] < 0x20)) {
    		cc.cc1[0] = ' ';
    		cc.cc1[1] = 0;
    	}
    */


    hi = cc.cc1[0];
    lo = cc.cc1[1];


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
            cc.cc1[0] = 0x20;
            cc.cc1[1] = 0x00;
        }
//          handle_text_attr (hi,lo,wb);
        if (lo>=0x30 && lo<=0x3f)
        {
            cc.cc1[0] = 0x20;
            cc.cc1[1] = 0x00;
//	wrote_to_screen=1;
//          handle_double (hi,lo,wb);
        }
        if (lo>=0x40 && lo<=0x7f)
        {
//          handle_pac (hi,lo,wb);
            cc.cc1[0] = 0x20;
            cc.cc1[1] = 0x00;
        }
        break;
    case 0x12:
    case 0x13:
        if (lo>=0x20 && lo<=0x3f)
        {
            cc.cc1[0] = 0x20;
            cc.cc1[1] = 0x00;
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







    if ((cc.cc1[0] >= 0x20) && (cc.cc1[0] < 0x80))
    {
        if ((current_cc_type == ROLLUP) || (current_cc_type == PAINTON))
        {
            cc_on_screen = true;
        }
        else if (current_cc_type == POPON)
        {
            cc_in_memory = true;
        }

        Debug(11, "%i:%i) %i:'%c':%x\t", cc_text_count, cc_text[cc_text_count].text_len, i, charmap[cc.cc1[0] - 0x20], cc.cc1[0]);
        cc_text[cc_text_count].text[cc_text[cc_text_count].text_len] = charmap[cc.cc1[0] - 0x20];
        cc_text[cc_text_count].text_len++;
        cc_text[cc_text_count].text[cc_text[cc_text_count].text_len] = '\0';
        if ((cc.cc1[1] >= 0x20) && (cc.cc1[1] < 0x80))
        {
            Debug(11, "%i:%i) %i:'%c':%x\t", cc_text_count, cc_text[cc_text_count].text_len, i, charmap[cc.cc1[1] - 0x20], cc.cc1[1]);
            cc_text[cc_text_count].text[cc_text[cc_text_count].text_len] = charmap[cc.cc1[1] - 0x20];
            cc_text[cc_text_count].text_len++;
            cc_text[cc_text_count].text[cc_text[cc_text_count].text_len] = '\0';
            if ((last_cc_type == ROLLUP) || (last_cc_type == PAINTON))
            {
                cc_on_screen = true;
            }
            else if (last_cc_type == POPON)
            {
                cc_in_memory = true;
            }
        }
    }

    if (((!isalpha(cc_text[cc_text_count].text[cc_text[cc_text_count].text_len - 1])) && (cc_text[cc_text_count].text_len > 200)) ||
            (cc_text[cc_text_count].text_len > 245))
    {
        cc_text[cc_text_count].end_frame = current_frame - 1;
        cc_text_count++;
        InitializeCCTextArray(cc_text_count);
        cc_text[cc_text_count].start_frame = current_frame;
        cc_text[cc_text_count].text_len = 0;
    }

    if (cc.cc1[0] == 0x14)
    {
        if ((cc.cc1[0] == lastcc.cc1[0]) && (cc.cc1[1] == lastcc.cc1[1]))
        {
            Debug(11, "Double code found\n");
            return;
        }

        switch (cc.cc1[1])
        {
        case 0x20:
            Debug(11, "Frame - %6i Control Code Found:\tResume Caption Loading\n", current_frame);
            cc_text[cc_text_count].end_frame = current_frame - 1;
            cc_text_count++;
            InitializeCCTextArray(cc_text_count);
            cc_text[cc_text_count].start_frame = current_frame;
            cc_text[cc_text_count].text_len = 0;
            last_cc_type = POPON;
            current_cc_type = POPON;
            AddNewCCBlock(current_frame, current_cc_type, cc_on_screen, cc_in_memory);
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
            Debug(11, "Frame - %6i Control Code Found:\tRoll Up Captions 2 row\n", current_frame);
            cc_text[cc_text_count].end_frame = current_frame - 1;
            cc_text_count++;
            InitializeCCTextArray(cc_text_count);
            cc_text[cc_text_count].start_frame = current_frame;
            cc_text[cc_text_count].text_len = 0;
            last_cc_type = ROLLUP;
            current_cc_type = ROLLUP;
            AddNewCCBlock(current_frame, current_cc_type, cc_on_screen, cc_in_memory);
            break;

        case 0x26:
            Debug(11, "Frame - %6i Control Code Found:\tRoll Up Captions 3 row\n", current_frame);
            cc_text[cc_text_count].end_frame = current_frame - 1;
            cc_text_count++;
            InitializeCCTextArray(cc_text_count);
            cc_text[cc_text_count].start_frame = current_frame;
            cc_text[cc_text_count].text_len = 0;
            last_cc_type = ROLLUP;
            current_cc_type = ROLLUP;
            AddNewCCBlock(current_frame, current_cc_type, cc_on_screen, cc_in_memory);
            break;

        case 0x27:
            Debug(11, "Frame - %6i Control Code Found:\tRoll Up Captions 4 row\n", current_frame);
            cc_text[cc_text_count].end_frame = current_frame - 1;
            cc_text_count++;
            InitializeCCTextArray(cc_text_count);
            cc_text[cc_text_count].start_frame = current_frame;
            cc_text[cc_text_count].text_len = 0;
            last_cc_type = ROLLUP;
            current_cc_type = ROLLUP;
            AddNewCCBlock(current_frame, current_cc_type, cc_on_screen, cc_in_memory);
            break;

        case 0x28:
            // Debug(11, "Frame - %6i Control Code
            // Found:\tFlash On\n", current_frame);
            break;

        case 0x29:
            Debug(11, "Frame - %6i Control Code Found:\tResume Direct Captioning\n", current_frame);
            cc_text[cc_text_count].end_frame = current_frame - 1;
            cc_text_count++;
            InitializeCCTextArray(cc_text_count);
            cc_text[cc_text_count].start_frame = current_frame;
            cc_text[cc_text_count].text_len = 0;
            last_cc_type = PAINTON;
            current_cc_type = PAINTON;
            AddNewCCBlock(current_frame, current_cc_type, cc_on_screen, cc_in_memory);
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
            Debug(11, "Frame - %6i Control Code Found:\tErase Displayed Memory\n", current_frame);
            cc_text[cc_text_count].end_frame = current_frame - 1;
            cc_text_count++;
            InitializeCCTextArray(cc_text_count);
            cc_text[cc_text_count].start_frame = current_frame;
            cc_text[cc_text_count].text_len = 0;
            cc_on_screen = false;
            current_cc_type = NONE;
            AddNewCCBlock(current_frame, current_cc_type, cc_on_screen, cc_in_memory);
            break;

        case 0x2D:
            // Debug(11, "Frame - %6i Control Code
            // Found:\tCarriage Return\n", current_frame);
            if (cc_text[cc_text_count].text_len > 200)
            {
                cc_text[cc_text_count].end_frame = current_frame - 1;
                cc_text_count++;
                InitializeCCTextArray(cc_text_count);
                cc_text[cc_text_count].start_frame = current_frame;
                cc_text[cc_text_count].text_len = 0;
            }

            cc_text[cc_text_count].text[cc_text[cc_text_count].text_len] = ' ';
            cc_text[cc_text_count].text_len++;
            cc_text[cc_text_count].text[cc_text[cc_text_count].text_len] = '\0';
            Debug(11, "\n");
            break;

        case 0x2E:
            Debug(11, "Frame - %6i Control Code Found:\tErase Non-Displayed Memory\n", current_frame);

            // cc_text_count++;
            // InitializeCCTextArray(cc_text_count);
            cc_in_memory = false;
            break;

        case 0x2F:
            Debug(
                11,
                "Frame - %6i Control Code Found:\tEnd of Caption\tOn Screen - %i\tOff Screen - %i\n",
                current_frame,
                cc_in_memory,
                cc_on_screen
            );
            cc_text[cc_text_count].end_frame = current_frame - 1;
            cc_text_count++;
            InitializeCCTextArray(cc_text_count);
            cc_text[cc_text_count].start_frame = current_frame;
            cc_text[cc_text_count].text_len = 0;
            tempBool = cc_in_memory;
            cc_in_memory = cc_on_screen;
            cc_on_screen = tempBool;
            if (!cc_on_screen)
            {
                current_cc_type = NONE;
            }
            else
            {
                if ((cc_block_count > 0) && (cc_block[cc_block_count].type == NONE))
                {
                    current_cc_type = last_cc_type;
                }
            }

            AddNewCCBlock(current_frame, current_cc_type, cc_on_screen, cc_in_memory);
            break;

        default:
            Debug(11, "\nFrame - %6i Control Code Found:\tUnknown code!! - %2X\n", current_frame, cc.cc1[1]);
            if (cc_text[cc_text_count].text_len > 200)
            {
                cc_text[cc_text_count].end_frame = current_frame - 1;
                cc_text_count++;
                InitializeCCTextArray(cc_text_count);
                cc_text[cc_text_count].start_frame = current_frame;
                cc_text[cc_text_count].text_len = 0;
            }

            cc_text[cc_text_count].text[cc_text[cc_text_count].text_len] = ' ';
            cc_text[cc_text_count].text_len++;
            cc_text[cc_text_count].text[cc_text[cc_text_count].text_len] = '\0';
            break;
        }
    }

    lastcc.cc1[0] = cc.cc1[0];
    lastcc.cc1[1] = cc.cc1[1];

}

void ProcessCCData(void)
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

    if (!initialized) return;

    // Reset state on the first frame
    if (framenum == 0) {
        last_cc_type = NONE;
        current_cc_type = NONE;
        cc_on_screen = false;
        cc_in_memory = false;
    }

    if (verbose >= 12)
    {
        p = (unsigned char *)temp;
        for (i = 0; i < ccDataLen; i++)
        {
            t = ccData[i] & 0x7f;
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
        if (ccData[0] == 'G')
            temp[7*3] = '0' + (temp[7*3] & 0x03);
        Debug(10, "CCData for framenum %4i%c, length:%4i: %s\n", framenum, pict_type, ccDataLen, temp);

        p = (unsigned char *)temp;
        for (i = 0; i < ccDataLen; i++)
        {
            sprintf(hex, "%2x ",ccData[i]);
            *p++ = hex[0];
            *p++ = hex[1];
            *p++ = ' ';
        }
        *p++ = 0;
        Debug(10, "CCData for framenum %4i%c, length:%4i: %s\n", framenum, pict_type, ccDataLen, temp);

    }

    if ((char)ccData[0] == 'C' && (char)ccData[1] == 'C' && ccData[2] == 0x01 && ccData[3] == 0xf8)
    {
        reorderCC = 0;
        packetCount = ccData[4];
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
            Debug(11, "CC Field Order: %i.  There appear to be %i packets.\n", cc1First, packetCount);
        }
        proceed = 1;
        is_CC = 1;
    }
    else 	if ((char)ccData[0] == 'G' && (char)ccData[1] == 'A' && ccData[2] == '9' && ccData[3] == '4'&& ccData[4] == 0x03)
    {
        reorderCC = 1;
        packetCount = ccData[5] & 0x1F;
        proceed = ( ccData[5] & 0x40) >> 6;
        offset = 7;
        is_GA = 1;
    }
    else 	if (ccData[0] == 0x05 && ccData[1] == 0x02)
    {
        reorderCC = 0;
        proceed = 0;
        offset = 7;
        cctype = ccData[offset++];
        if (cctype == 2)
        {
            offset++;
            cc.cc1[0] = ccData[offset++];
            cc.cc1[1] = ccData[offset++];
            AddCC(1);
            cctype = ccData[offset++];
            if (cctype == 4 && ( ccData[offset] & 0x7f) < 32)
            {
                cc.cc1[0] = ccData[offset++];
                cc.cc1[1] = ccData[offset++];
                AddCC(1);
            }
            offset += 3;
        }
        else if (cctype == 4)
        {
            offset++;
            cc.cc1[0] = ccData[offset++];
            cc.cc1[1] = ccData[offset++];
            AddCC(1);
            cc.cc1[0] = ccData[offset++];
            cc.cc1[1] = ccData[offset++];
            AddCC(1);
            offset += 3;
        }
        else if (cctype == 5)
        {
            for (i = 0; i < prevccDataLen; i +=2)
            {
                cc.cc1[0] = prevccData[i];
                cc.cc1[1] = prevccData[i+1];
                AddCC(i/2);
            }
            prevccDataLen = 0;
//			offset += 6;
            cctype = ccData[offset++] & 0x7f;
            cctype = ccData[offset++] & 0x7f;
            cctype = ccData[offset++] & 0x7f;
            cctype = ccData[offset++] & 0x7f;
            cctype = ccData[offset++] & 0x7f;
            cctype = ccData[offset++] & 0x7f;
//
            cctype = ccData[offset++];
            offset++;
            prevccDataLen = 0;
            prevccData[prevccDataLen++] = ccData[offset++];
            prevccData[prevccDataLen++] = ccData[offset++];
            if (cctype == 2)
            {
                cctype = ccData[offset++];
                if (cctype == 4 && ( ccData[offset] & 0x7f) < 32)
                {
                    prevccData[prevccDataLen++] = ccData[offset++];
                    prevccData[prevccDataLen++] = ccData[offset++];
                }
            }
            else
            {
                prevccData[prevccDataLen++] = ccData[offset++];
                prevccData[prevccDataLen++] = ccData[offset++];
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
                    cc.cc1[0] = CheckOddParity(ccData[(i * 6) + offset + 1]) ? ccData[(i * 6) + offset + 1] & 0x7f : 0x00;
                    cc.cc1[1] = CheckOddParity(ccData[(i * 6) + offset + 2]) ? ccData[(i * 6) + offset + 2] & 0x7f : 0x00;
                }
                else
                {
                    cc.cc1[0] = CheckOddParity(ccData[(i * 6) + offset + 4]) ? ccData[(i * 6) + offset + 4] & 0x7f : 0x00;
                    cc.cc1[1] = CheckOddParity(ccData[(i * 6) + offset + 5]) ? ccData[(i * 6) + offset + 5] & 0x7f : 0x00;
                }
                AddCC(i);
            }
            if (is_GA)
            {

                if (!(ccData[(i * 3) + offset] & 4) >>2 )
                    continue;
                if (ccData[(i * 3) + offset] == 0xfa)
                    continue;
                if (ccData[(i * 3) + offset + 1]  == 0x80 && ccData[(i * 3) + offset + 2] == 0x80)
                    continue;
                if (ccData[(i * 3) + offset + 1]  == 0x00 && ccData[(i * 3) + offset + 2] == 0x00)
                    continue;

                cctype = (ccData[(i * 3) + offset] & 3);
//				cc.cc1[0] = CheckOddParity(ccData[(i * 3) + offset + 1]) ? ccData[(i * 3) + offset + 1] & 0x7f : 0x00;
//				cc.cc1[1] = CheckOddParity(ccData[(i * 3) + offset + 2]) ? ccData[(i * 3) + offset + 2] & 0x7f : 0x00;
                cc.cc1[0] = ccData[(i * 3) + offset + 1] & 0x7f;
                cc.cc1[1] = ccData[(i * 3) + offset + 2] & 0x7f;

                /*
                if (cctype == 0)
                    cctype = cctype;
                */
                if (cctype == 1)
                    AddXDS(ccData[(i * 3) + offset + 1], ccData[(i * 3) + offset + 2]);
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
                    AddCC(i);

                }
                else
                {
                    cc.cc1[0] = 0;
                    cc.cc1[1] = 0;
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

void AddNewCCBlock(long current_frame, int type, bool cc_on_screen, bool cc_in_memory)
{
    if (cc_block[cc_block_count].type == type)
    {
        cc_block[cc_block_count].end_frame = current_frame;
    }
    else
    {
        Debug(11, "\nFrame - %6i\t%s captions start\n", current_frame, CCTypeToStr(type));
        if (cc_block[cc_block_count].end_frame == -1)
        {
            Debug(11, "New cblock found\n");
            cc_block[cc_block_count].end_frame = current_frame - 1;
            cc_block_count++;
            InitializeCCBlockArray(cc_block_count);
            cc_block[cc_block_count].start_frame = current_frame;
            cc_block[cc_block_count].type = type;
            if (cc_block_count > 1)
            {
                if ((F2L(cc_block[cc_block_count - 1].end_frame, cc_block[cc_block_count - 1].start_frame) < 1.0) &&
                        (cc_block[cc_block_count].type == cc_block[cc_block_count - 2].type) &&
                        (cc_block[cc_block_count].type != NONE))
                {
                    cc_block_count -= 2;
                    cc_block[cc_block_count].end_frame = -1;
                }
            }
        }
        else
        {
            cc_block_count++;
            InitializeCCBlockArray(cc_block_count);
            cc_block[cc_block_count].start_frame = current_frame;
            cc_block[cc_block_count].type = type;
        }

        OutputCCBlock(cc_block_count);
    }
}

char* CCTypeToStr(int type)
{
    if (processCC)
    {
        switch (type)
        {
        case NONE:
            sprintf(tempString, "NONE");
            break;

        case ROLLUP:
            sprintf(tempString, "ROLLUP");
            break;

        case PAINTON:
            sprintf(tempString, "PAINTON");
            break;

        case POPON:
            sprintf(tempString, "POPON");
            break;

        case COMMERCIAL:
            sprintf(tempString, "COMMERCIAL");
            break;

        default:
            sprintf(tempString, "%d",type);
            break;
        }
    }
    else
    {
        tempString[0]=0; // was: sprintf(tempString, "");
    }

    return (tempString);
}

int DetermineCCTypeForBlock(long start, long end)
{
    int type = NONE;
    int i = 0;
    int j = 0;
    int cc_block_first = cc_block_count;
    int cc_block_last = 0;
    int cc_type_count[5] = { 0, 0, 0, 0, 0 };
    while (cc_block[cc_block_first].start_frame > start) cc_block_first--;
    while (cc_block[cc_block_last].end_frame < end) cc_block_last++;

    // Look for the PAINTON then POPON pattern that is common in commercials
    for (i = cc_block_first; i <= cc_block_last; i++)
    {
        if (cc_block[i].type != NONE)
        {
            if (i > 0)
            {
                if ((cc_block[i - 1].type == PAINTON) && (cc_block[i].type == POPON))
                {
 //                   type = COMMERCIAL;
                    break;
                }
            }

            if (i > 1)
            {
                if ((cc_block[i - 2].type == PAINTON) &&
                        (cc_block[i - 1].type == NONE) &&
                        (F2L(cc_block[i - 1].end_frame, cc_block[i - 1].start_frame) <= 1.5) &&
                        (cc_block[i].type == POPON))
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
            for (j = 0; j < cc_block_count; j++)
            {
                if ((i > cc_block[j].start_frame) && (i < cc_block[j].end_frame))
                {
                    cc_type_count[cc_block[j].type]++;
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

    Debug(4, "Start - %6i\tEnd - %6i\tCCF - %2i\tCCL - %2i\tType - %s\n", start, end, cc_block_first, cc_block_last, CCTypeToStr(type));

    return (type);
}



void SetARofBlocks(void)
{
    int		i, j,k;
    double	sumAR = 0.0;
    int		frameCount = 0;
    if (!(commDetectMethod & AR))
        return;
    k = 0;
    for (i = 0; i < block_count; i++)
    {
        sumAR = 0.0;
        frameCount = 0; // To prevent divide by zero error
        for (j = cblock[i].f_start + cblock[i].b_head;
                j < cblock[i].f_end - (int) cblock[i].b_tail; j++)
        {
            if ( k < ar_block_count && j >= ar_block[k].end )
                k++;
            if (ar_block[k].ar_ratio > 1)
            {
                sumAR += ar_block[k].ar_ratio;
                frameCount++;
            }
        }
        if (frameCount == 0)
            cblock[i].ar_ratio = 1.0;
        else
            cblock[i].ar_ratio = sumAR / (frameCount);
    }
}



bool ProcessCCDict(void)
{
    int		i, j;
    char*	ptr;
    char	phrase[1024];
    bool	goodPhrase = true;
    FILE*	dict = NULL;
    dict = myfopen(dictfilename, "r");
    if (dict == NULL)
    {
        return (false);
    }

    Debug(2, "\n\nStarting to process dictionary\n-------------------------------------\n");
    while (fgets(phrase, sizeof(phrase), dict) != NULL)
    {
        ptr = strchr(phrase, '\n');
        if (ptr != NULL) *ptr = '\0';
        if (strstr(phrase, "-----") != NULL)
        {
            goodPhrase = false;
            Debug(3, "Finished with good phrases.  Now starting bad phrases.\n");
            continue;
        }
        // just in case the line is empty
        if (strlen(phrase) < 1) continue;

        Debug(3, "Searching for: %s\n", phrase);
        for (i = 0; i < cc_text_count; i++)
        {
            if (strstr(_strupr((char*)cc_text[i].text), _strupr((char*)phrase)) != NULL)
            {
                Debug(2, "%s found in cc_text_block %i\n", phrase, i);
                if (goodPhrase)
                {
                    j = FindBlock((cc_text[i].start_frame + cc_text[i].end_frame) / 2);
                    if (j == -1)
                    {
                        Debug(1, "There was an error finding the correct cblock for cc text cblock %i.\n", i);
                    }
                    else
                    {
                        Debug(3, "Block %i score:\tBefore - %.2f\t", j, cblock[j].score);
                        cblock[j].score /= dictionary_modifier;
                        Debug(3, "After - %.2f\n", cblock[j].score);
                    }
                }
                else
                {
                    j = FindBlock((cc_text[i].start_frame + cc_text[i].end_frame) / 2);
                    if (j == -1)
                    {
                        Debug(1, "There was an error finding the correct cblock for cc text cblock %i.\n", i);
                    }
                    else
                    {
                        Debug(3, "Block %i score:\tBefore - %.2f\t", j, cblock[j].score);
                        cblock[j].score *= dictionary_modifier;
                        Debug(3, "After - %.2f\n", cblock[j].score);
                    }
                }
            }
        }
    }

    fclose(dict);
    return (true);
}

