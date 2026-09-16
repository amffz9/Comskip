#include "diagnostic.h"
#include "output/legacy_editor_exports.h"
#include <cmath>
#include <cstdint>
#include <format>
#include <ostream>
#include <stdexcept>
namespace comskip::output {
namespace {
void time(EditorSeconds at) {
    if (!std::isfinite(at.count()) || at.count()<0)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_legacy_editor_interval);
    if (at.count()>=std::ldexp(1.0,63)/10000000)
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::legacy_editor_timestamp_exceeds_range);
}
void validate(std::span<const EditorInterval> cuts) {
    for(const auto& cut:cuts){time(cut.start);time(cut.end);
        if(cut.end<cut.start)throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_legacy_editor_interval);}
}
std::string vdr_time(EditorSeconds at,double fps) {
    double seconds=at.count();
    const auto hours=static_cast<std::int64_t>(seconds/3600);seconds-=hours*3600;
    const auto minutes=static_cast<int>(seconds/60);seconds-=minutes*60;
    const auto whole=static_cast<int>(seconds);
    const auto centiseconds=static_cast<int>((seconds-whole)*100);
    const auto frame=static_cast<int>(centiseconds*fps/100);
    return std::format("{}:{:02}:{:02}.{:02}",hours,minutes,whole,frame);
}
void finish(std::ostream& out){if(!out)throw comskip::diagnostics::DiagnosticError<std::runtime_error>(comskip::diagnostics::Code::cannot_write_legacy_editor_export);}
}
void write_vdr(std::ostream& out,std::span<const EditorInterval> cuts,double fps) {
    if(!std::isfinite(fps)||fps<=0||fps>=std::ldexp(1.0,31))
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_legacy_editor_geometry);
    validate(cuts);
    for(const auto& cut:cuts)out<<vdr_time(cut.start,fps)<<" start\n"<<vdr_time(cut.end,fps)<<" end\n";
    finish(out);
}
void write_videoredo2(std::ostream& out,VideoRedo2Options options,std::span<const EditorInterval> cuts,std::span<const EditorScene> scenes) {
    validate(cuts);
    for(const auto& scene:scenes){time(scene.at);if(scene.number<0)throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_legacy_editor_scene);}
    out<<"<Version>2\n<Filename>"<<options.filename<<'\n';
    if(options.h264)out<<"<MPEG Stream Type>4\n";
    if(options.streams)out<<std::format("<VideoStreamPID>{}\n<AudioStreamPID>{}\n<SubtitlePID1>{}\n",
        options.streams->video,options.streams->audio,options.streams->subtitle);
    for(const auto& cut:cuts)out<<std::format("<Cut>{:.0f}:{:.0f}\n",cut.start.count()*10000000,cut.end.count()*10000000);
    for(const auto& scene:scenes)out<<std::format("<SceneMarker {}>{:.0f}\n",scene.number,scene.at.count()*10000000);
    finish(out);
}
}
