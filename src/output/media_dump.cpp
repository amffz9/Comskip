#include "diagnostic.h"
#include "legacy_detection.h"
#include "output/media_dump.h"
#include <format>
#include <stdexcept>
#include <string>



void dump_audio_start(RecordingContext& context)
{
    if (!context.settings.output_demux) return;
    if (!context.state.dump_audio_file.get())
    {
        const auto filename = std::string(context.state.workbasename) + ".mp2";
        context.state.dump_audio_file.reset(myfopen(filename.c_str(), "wb"));
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
    if (!context.settings.output_demux) return;
    if (!context.state.dump_video_file.get())
    {
        const auto filename = std::string(context.state.workbasename) + ".m2v";
        context.state.dump_video_file.reset(myfopen(filename.c_str(), "wb"));
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
    context.state.dump_audio_file.reset();
    context.state.dump_video_file.reset();
}


void dump_data(RecordingContext& context, char *start, int length)
{
    if (!context.settings.output_data) return;
    if (length < 0 || (length > 0 && !start))
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_data_dump_buffer);
    if (!length) return;
    if (length > 1900) return;
    if (context.state.framenum_real < 0 || context.state.framenum_real > 9999999)
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::data_dump_frame_number_exceeds_field);
    if (!context.state.dump_data_file.get())
    {
        const auto filename = std::string(context.state.workbasename) + ".data";
        context.state.dump_data_file.reset(myfopen(filename.c_str(), "wb"));
        if (!context.state.dump_data_file) {
            Debug(context, 1, "%s", context.translator.format("create_failed", strerror(errno), filename).c_str());
            return;
        }
    }
    auto record = std::format("{:7}:{:4}", context.state.framenum_real, length);
    record.append(start, static_cast<std::size_t>(length));
    if (fwrite(record.data(), 1, record.size(), context.state.dump_data_file.get()) != record.size())
        Debug(context, 1, "%s", context.translator.text("diagnostics_dump_write_failed"));
}

void close_data(RecordingContext& context)
{
    context.state.dump_data_file.reset();
}
