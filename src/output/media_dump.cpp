#include "legacy_detection.h"



void dump_audio_start(RecordingContext& context)
{
    char temp[256];
    if (!context.settings.output_demux) return;
    if (!context.state.dump_audio_file.get())
    {
        sprintf(temp, "%s.mp2", context.state.workbasename);
        context.state.dump_audio_file.reset(myfopen(temp, "wb"));
    }
}

void dump_audio (RecordingContext& context, char *start, char *end)
{
    if (!context.settings.output_demux) return;
    if (!context.state.dump_audio_file.get()) return;

    fwrite(start, end-start, 1, context.state.dump_audio_file.get());
//	fclose(dump_audio_file);
}



void dump_video_start(RecordingContext& context)
{
    char temp[256];
    if (!context.settings.output_demux) return;
    if (!context.state.dump_video_file.get())
    {
        sprintf(temp, "%s.m2v", context.state.workbasename);
        context.state.dump_video_file.reset(myfopen(temp, "wb"));
    }
}
void dump_video (RecordingContext& context, char *start, char *end)
{
    if (!context.settings.output_demux) return;
    if (!context.state.dump_video_file.get()) return;
    fwrite(start, end-start, 1, context.state.dump_video_file.get());
//	fclose(dump_video_file);
}

void close_dump(RecordingContext& context)
{
    if (!context.settings.output_demux) return;
    if (context.state.dump_audio_file.get())
    {
        context.state.dump_audio_file.reset();
    }
    context.state.dump_audio_file.reset();
    if (context.state.dump_video_file.get())
    {
        context.state.dump_video_file.reset();
    }
    context.state.dump_video_file.reset();
}


void dump_data(RecordingContext& context, char *start, int length)
{
    char temp[2000];
    int i;
    if (!context.settings.output_data) return;
    if (!length) return;
    if (!context.state.dump_data_file.get())
    {
        sprintf(temp, "%s.data", context.state.workbasename);
        context.state.dump_data_file.reset(myfopen(temp, "wb"));
    }
    if (length > 1900)
        return;
    sprintf(temp, "%7d:%4d",context.state.framenum_real, length);
    for (i=0; i<length; i++)
        temp[i+12] = start[i] & 0xff;
    fwrite(temp, length+12, 1, context.state.dump_data_file.get());

//	fclose(dump_data_file);
}

void close_data(RecordingContext& context)
{
    if (context.settings.output_data)
    {
    if (context.state.dump_data_file.get()) {
        context.state.dump_data_file.reset();
        context.state.dump_data_file.reset();
    }
    }
}
