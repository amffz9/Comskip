#include "diagnostic.h"
#include "output/legacy_editor_adapter.h"
#include "output/legacy_editor_exports.h"
#include "output/output_file.h"
#include "detection/frame_timestamps.h"
#include "recording_context.h"
#include "platform/utf8_paths.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <vector>
void WriteLegacyEditorFiles(RecordingContext& context,bool use_reference) {
    using namespace comskip::output;
    const auto& state=context.state;const auto& settings=context.settings;
    const bool project=settings.output_videoredo&&!settings.output_videoredo3;
    if(!settings.output_vdr&&!project)return;
    const int count=use_reference?state.reffer_count:state.commercial_count;
    if(use_reference)comskip::detection::validate_intervals(state.reffer,count);
    else comskip::detection::validate_intervals(state.commercial,count);
    if(!std::isfinite(settings.fps)||settings.fps<=0||state.frame_count<2||
        state.block_count<0||static_cast<std::size_t>(state.block_count)>state.cblock.size()||
        (!state.frame.empty()&&(state.framenum_real<2||static_cast<std::size_t>(state.framenum_real)>state.frame.size())))
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_legacy_editor_geometry);
    const auto at=[&](std::int64_t frame){
        if(frame>std::numeric_limits<int>::max())
            throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::legacy_editor_timestamp_exceeds_range);
        return EditorSeconds{get_frame_pts(context,static_cast<int>(frame))};
    };
    const auto offset=[&](long frame){return std::max<std::int64_t>(static_cast<std::int64_t>(frame)-settings.videoredo_offset-1,0);};
    std::vector<EditorInterval> vdr,cuts;std::vector<EditorScene> scenes;
    std::optional<EditorStreamIds> streams;
    const auto append=[&](int index,long previous,long start,long end){
        if(previous<start&&end-start>2){
            vdr.push_back({at(start<5?0:start),at(end)});
            cuts.push_back({at(offset(start)),at(offset(end))});
            if(index==0&&state.demux_pid)streams=EditorStreamIds{state.selected_video_pid,state.selected_audio_pid,state.selected_subtitle_pid};
        }
    };
    long previous=-1;
    for(int i=0;i<=count;++i){
        const auto start=use_reference?state.reffer[i].start_frame:state.commercial[i].start_frame;
        const auto end=use_reference?state.reffer[i].end_frame:state.commercial[i].end_frame;
        if(start<0||end<start||start<=previous||start>=state.frame_count||end>state.frame_count)
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_legacy_editor_commercial_range);
        append(i,previous,start,end);previous=end;
    }
    if(count<0||previous<state.frame_count-2)append(count+1,previous,state.frame_count-2,state.frame_count-1);
    for(int i=0;project&&i<state.block_count;++i){
        const auto frame=offset(state.cblock[i].f_end);
        if(state.cblock[i].f_end<0)throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_legacy_editor_scene);
        const double value=state.frame.empty()?static_cast<double>(frame)/settings.fps:
            state.frame[frame<=0?1:std::min<std::int64_t>(frame,state.framenum_real-1)].pts;
        scenes.push_back({i,EditorSeconds{value}});
    }
    const auto write=[&](const char* extension,auto serializer){
        std::ostringstream contents;serializer(contents);
        const auto filename=state.outbasename+extension;
        write_output_file(filename,contents.str());
    };
    if(settings.output_vdr)write(".vdr",[&](auto& out){write_vdr(out,vdr,settings.fps);});
    if(project){
        const auto filename=comskip::platform::path_to_utf8(std::filesystem::absolute(comskip::platform::path_from_utf8(state.mpegfilename)));
        write(".VPrj",[&](auto& out){write_videoredo2(out,{filename,state.is_h264!=0,streams},cuts,scenes);});
    }
}
