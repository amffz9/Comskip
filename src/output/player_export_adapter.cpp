#include "diagnostic.h"
#include "output/player_export_adapter.h"
#include "output/player_exports.h"
#include "output/output_file.h"
#include "detection/frame_timestamps.h"
#include "recording_context.h"
#include "platform/utf8_paths.h"
#include <cmath>
#include <limits>
#include <sstream>
#include <vector>

void WritePlayerExportFiles(RecordingContext& context,bool use_reference) {
    using namespace comskip::output;
    const auto& state=context.state;
    const auto& settings=context.settings;
    if (!settings.output_zoomplayer_cutlist && !settings.output_zoomplayer_chapter &&
        !settings.output_scf && !settings.output_ipodchap && !settings.output_bsplayer) return;
    const int count=use_reference ? state.reffer_count : state.commercial_count;
    if (use_reference) comskip::detection::validate_intervals(state.reffer,count);
    else comskip::detection::validate_intervals(state.commercial,count);
    if (!std::isfinite(settings.fps) || settings.fps<=0 || settings.fps+0.5>=std::numeric_limits<int>::max() || state.frame_count<2)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_player_export_media_geometry);
    const int rounded_fps=static_cast<int>(settings.fps+0.5);
    std::vector<PlayerInterval> intervals;
    std::vector<PlayerChapterMark> chapters,ipod;
    std::vector<ScfFrameMark> scf;
    const auto at=[&](long frame) {return PlayerSeconds{get_frame_pts(context,frame)};};
    const auto append=[&](int index,long previous,long start,long end) {
        if (previous>=start) return;
        if (end-start>2) {
            intervals.push_back({at(start),at(end)});
            ipod.push_back({at(end),static_cast<std::uint32_t>(index)+2,PlayerBoundary::commercial_end});
        }
        if (end-start>settings.fps) {
            const auto number=static_cast<std::uint32_t>(index)*2+1;
            chapters.push_back({at(start),number,PlayerBoundary::commercial_start});
            chapters.push_back({at(end),number+1,PlayerBoundary::commercial_end});
            scf.push_back({start,number,PlayerBoundary::commercial_start});
            scf.push_back({end,number+1,PlayerBoundary::commercial_end});
        }
    };
    long previous=-1;
    bool initial_show=false;
    for (int i=0;i<=count;++i) {
        const auto start=use_reference ? state.reffer[i].start_frame : state.commercial[i].start_frame;
        const auto end=use_reference ? state.reffer[i].end_frame : state.commercial[i].end_frame;
        if (start<0 || end<start || start<=previous || start>=state.frame_count || end>state.frame_count)
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_player_export_commercial_range);
        if (i==0) initial_show=start>5;
        append(i,previous,start,end); previous=end;
    }
    if (count<0 || previous<state.frame_count-2) append(count+1,previous,state.frame_count-2,state.frame_count-1);
    const auto write=[&](const char* extension,auto serialize) {
        std::ostringstream contents; serialize(contents);
        const auto filename=state.outbasename+extension;
        write_output_file(filename, contents.str());
    };
    if (settings.output_zoomplayer_chapter) write(".chp",[&](auto& out){write_zoomplayer_chapters(out,chapters,initial_show);});
    if (settings.output_zoomplayer_cutlist) write(".cut",[&](auto& out){write_zoomplayer_cuts(out,intervals);});
    if (settings.output_scf) write(".scf",[&](auto& out){write_scf(out,scf,{rounded_fps});});
    if (settings.output_ipodchap) write(settings.output_chapters ? ".ipod.chap" : ".chap",
        [&](auto& out){write_ipod_chapters(out,ipod);});
    if (settings.output_bsplayer) write(".bcf",[&](auto& out){write_bsplayer(out,intervals);});
}
