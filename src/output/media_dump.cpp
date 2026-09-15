#include "legacy_detection.h"

FILE *dump_audio_file;

void dump_audio_start()
{
    char temp[256];
    if (!output_demux) return;
    if (!dump_audio_file)
    {
        sprintf(temp, "%s.mp2", workbasename);
        dump_audio_file = myfopen(temp, "wb");
    }
}

void dump_audio (char *start, char *end)
{
    if (!output_demux) return;
    if (!dump_audio_file) return;

    fwrite(start, end-start, 1, dump_audio_file);
//	fclose(dump_audio_file);
}

FILE *dump_video_file;

void dump_video_start()
{
    char temp[256];
    if (!output_demux) return;
    if (!dump_video_file)
    {
        sprintf(temp, "%s.m2v", workbasename);
        dump_video_file = myfopen(temp, "wb");
    }
}
void dump_video (char *start, char *end)
{
    if (!output_demux) return;
    if (!dump_video_file) return;
    fwrite(start, end-start, 1, dump_video_file);
//	fclose(dump_video_file);
}

void close_dump(void)
{
    if (!output_demux) return;
    if (dump_audio_file)
    {
        fclose(dump_audio_file);
    }
    dump_audio_file = NULL;
    if (dump_video_file)
    {
        fclose(dump_video_file);
    }
    dump_video_file = NULL;
}


void dump_data(char *start, int length)
{
    char temp[2000];
    int i;
    if (!output_data) return;
    if (!length) return;
    if (!dump_data_file)
    {
        sprintf(temp, "%s.data", workbasename);
        dump_data_file = myfopen(temp, "wb");
    }
    if (length > 1900)
        return;
    sprintf(temp, "%7d:%4d",framenum_real, length);
    for (i=0; i<length; i++)
        temp[i+12] = start[i] & 0xff;
    fwrite(temp, length+12, 1, dump_data_file);

//	fclose(dump_data_file);
}

void close_data()
{
    if (output_data)
    {
	if (dump_data_file) {
		fclose(dump_data_file);
		dump_data_file = 0;
	}
    }
}
