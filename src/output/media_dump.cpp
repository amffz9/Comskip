#include "diagnostic.h"
#include "app/recording_context.h"
#include "output/media_dump.h"
#include "output/checked_file.h"
#include "platform/platform.h"
#include <format>
#include <stdexcept>
#include <string>

namespace {
comskip::platform::FilePtr open_dump(std::string_view path) {
    auto file=comskip::platform::open_file_owned(path,"wb");
    if (!file)
        throw comskip::diagnostics::DiagnosticError<std::ios_base::failure>(
            comskip::diagnostics::Code::output_open,{std::string(path)});
    return file;
}
void write_dump(std::FILE& file, std::string_view path, std::span<const std::uint8_t> data) {
    if (std::fwrite(data.data(),1,data.size(),&file)!=data.size())
        throw comskip::diagnostics::DiagnosticError<std::ios_base::failure>(
            comskip::diagnostics::Code::output_write,{std::string(path)});
}
}

void dump_audio_start(RecordingContext& context)
{
    if (!context.settings.output_demux) return;
    if (!context.state.dump_audio_file.get())
    {
        const auto filename = std::string(context.state.workbasename) + ".mp2";
        context.state.dump_audio_file=open_dump(filename);
    }
}

void dump_audio(RecordingContext& context, std::span<const std::uint8_t> data)
{
    if (!context.settings.output_demux) return;
    if (!context.state.dump_audio_file.get()) return;
    write_dump(*context.state.dump_audio_file,std::string(context.state.workbasename)+".mp2",data);
}



void dump_video_start(RecordingContext& context)
{
    if (!context.settings.output_demux) return;
    if (!context.state.dump_video_file.get())
    {
        const auto filename = std::string(context.state.workbasename) + ".m2v";
        context.state.dump_video_file=open_dump(filename);
    }
}
void dump_video(RecordingContext& context, std::span<const std::uint8_t> data)
{
    if (!context.settings.output_demux) return;
    if (!context.state.dump_video_file.get()) return;
    write_dump(*context.state.dump_video_file,std::string(context.state.workbasename)+".m2v",data);
}

void close_dump(RecordingContext& context)
{
    comskip::output::checked_close(context.state.dump_audio_file,std::string(context.state.workbasename)+".mp2");
    comskip::output::checked_close(context.state.dump_video_file,std::string(context.state.workbasename)+".m2v");
}


void dump_data(RecordingContext& context, std::span<const std::uint8_t> data)
{
    if (!context.settings.output_data) return;
    if (data.empty()) return;
    if (data.size() > 1900) return;
    if (context.state.framenum_real < 0 || context.state.framenum_real > 9999999)
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::data_dump_frame_number_exceeds_field);
    if (!context.state.dump_data_file.get())
    {
        const auto filename = std::string(context.state.workbasename) + ".data";
        context.state.dump_data_file=open_dump(filename);
    }
    auto record = std::format("{:7}:{:4}", context.state.framenum_real, data.size());
    record.append(reinterpret_cast<const char*>(data.data()),data.size());
    write_dump(*context.state.dump_data_file,std::string(context.state.workbasename)+".data",
        {reinterpret_cast<const std::uint8_t*>(record.data()),record.size()});
}

void close_data(RecordingContext& context)
{
    comskip::output::checked_close(context.state.dump_data_file,std::string(context.state.workbasename)+".data");
}
