#include "output/xml_output_adapter.h"
#include "output/xml_cutlists.h"
#include "output/plist_cutlist.h"
#include "recording_context.h"
#include "exit_requested.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

void WriteXmlOutputFiles(RecordingContext& context, bool use_reference)
{
    using namespace comskip::output;
    if (!context.settings.output_videoredo3 && !context.settings.output_edlx &&
        !context.settings.output_btv && !context.settings.output_cuttermaran &&
        !context.settings.output_dvrmstb && !context.settings.output_mkvtoolnix &&
        !context.settings.output_plist_cutlist) return;

    const auto& state = context.state;
    const double fps = context.settings.fps;
    if (!std::isfinite(fps) || fps <= 0 || state.frame_count < 2)
        throw std::invalid_argument("Invalid XML media geometry");
    if (state.block_count < 0 || static_cast<std::size_t>(state.block_count) > std::size(state.cblock))
        throw std::out_of_range("Invalid XML detector block count");
    if (!state.frame.empty() && static_cast<std::size_t>(state.frame_count) > state.frame.size())
        throw std::out_of_range("XML timestamps exceed frame buffer");
    if (!state.frame.empty() &&
        (state.framenum_real < 2 || static_cast<std::size_t>(state.framenum_real) > state.frame.size()))
        throw std::out_of_range("XML detector timing exceeds frame buffer");

    const auto path = [](const std::string& bytes) {
        return std::filesystem::path(std::u8string(bytes.begin(), bytes.end()));
    };
    const XmlMediaDescription media{std::filesystem::absolute(path(context.state.mpegfilename)),
        path(context.state.inbasename), context.state.demux_pid
            ? std::optional<StreamIds>{{context.state.selected_video_pid,
                context.state.selected_audio_pid, context.state.selected_subtitle_pid}}
            : std::nullopt};
    const int count = use_reference ? context.state.reffer_count : context.state.commercial_count;
    if (use_reference) comskip::detection::validate_intervals(state.reffer, count);
    else comskip::detection::validate_intervals(state.commercial, count);
    std::vector<CommercialInterval> list;
    list.reserve(static_cast<std::size_t>(count + 1));
    for (int i = 0; i <= count; ++i) {
        if (use_reference) list.push_back({context.state.reffer[i].start_frame, context.state.reffer[i].end_frame});
        else list.push_back({context.state.commercial[i].start_frame, context.state.commercial[i].end_frame});
    }
    FrameIndex previous_end = -1;
    for (const auto& interval : list) {
        if (interval.start_frame < 0 || interval.end_frame < interval.start_frame ||
            interval.start_frame <= previous_end)
            throw std::invalid_argument("Invalid or overlapping commercial XML range");
        // Detector blocks and padded commercial lists can end at the terminal
        // frame_count boundary. It has no frame storage/timestamp of its own.
        if (interval.start_frame >= state.frame_count || interval.end_frame > state.frame_count)
            throw std::out_of_range("Commercial XML range exceeds media");
        previous_end = interval.end_frame;
    }
    previous_end = -1;
    for (long i = 0; i < state.block_count; ++i) {
        const auto& block = state.cblock[i];
        if (block.f_start < 0 || block.f_end < block.f_start || block.f_start <= previous_end)
            throw std::invalid_argument("Invalid or overlapping detector XML range");
        if (block.f_start >= state.frame_count || block.f_end > state.frame_count)
            throw std::out_of_range("Detector XML range exceeds media");
        previous_end = block.f_end;
    }
    std::vector<TimeInterval> cuts, btv, dvr, plist;
    std::vector<ByteInterval> bytes;
    std::vector<FrameInterval> retained;
    std::vector<SceneMarker> scenes;
    std::vector<ChapterSegment> chapters;
    long previous = -1;
    // Commercial timestamps clamp to the reported frame count. Scene/retained
    // timing historically clamps to the decoder's real count instead.
    const auto time = [&](FrameIndex frame) {
        if (state.frame.empty()) return Seconds{static_cast<double>(frame) / fps};
        return Seconds{state.frame[std::clamp<FrameIndex>(frame, 1, state.frame_count - 1)].pts};
    };
    const auto detector_time = [&](FrameIndex frame) {
        if (state.frame.empty()) return Seconds{static_cast<double>(frame) / fps};
        const auto index = frame <= 0 ? 1 : std::min<FrameIndex>(frame, state.framenum_real - 1);
        return Seconds{state.frame[index].pts};
    };
    const auto detector_frame = [&](FrameIndex frame) {
        const double position = detector_time(frame).count() * fps + 1.5;
        if (!std::isfinite(position) || position < 0 ||
            position >= static_cast<double>(std::numeric_limits<FrameIndex>::max()))
            throw std::out_of_range("Invalid retained XML frame position");
        return static_cast<FrameIndex>(position);
    };
    const auto append_retained = [&](long before) {
        if (previous + 1 < before)
            retained.push_back({detector_frame(previous + 1), detector_frame(before - 1)});
    };
    for (int i = 0; i <= count; ++i) {
        const long start = list[i].start_frame, end = list[i].end_frame;
        append_retained(start);
        if (previous < start) {
            btv.push_back({time(start), time(end)});
            plist.push_back({time(start), time(end)});
            if (end - start > 2) {
                cuts.push_back({time(std::max<FrameIndex>(static_cast<FrameIndex>(start) - context.settings.videoredo_offset - 1, 0)),
                                time(std::max<FrameIndex>(static_cast<FrameIndex>(end) - context.settings.videoredo_offset - 1, 0))});
                if (!context.state.frame.empty() && context.settings.output_edlx) {
                    const auto final_frame = std::min<long>(end, state.frame_count - 1);
                    bytes.push_back({state.frame[start].goppos, state.frame[final_frame].goppos});
                }
            }
        }
        if (end - start > 1) dvr.push_back({time(start == 1 ? 0 : start), time(end)});
        previous = end;
    }
    // The legacy final sentinel describes the retained tail, not a commercial.
    // Preserve its frame endpoint without serializing it as a false BTV cut.
    if (previous < context.state.frame_count - 2) append_retained(context.state.frame_count - 2);
    // Unlike the other XML formats, the historical plist includes the final
    // sentinel pair, even when there are no commercial marks.
    if (previous < context.state.frame_count - 2)
        plist.push_back({time(context.state.frame_count - 2), time(context.state.frame_count - 1)});
    for (int i = 0; i < context.state.block_count; ++i) {
        const auto index = std::max<FrameIndex>(static_cast<FrameIndex>(context.state.cblock[i].f_end) - context.settings.videoredo_offset - 1, 0);
        scenes.push_back({detector_time(index), static_cast<std::size_t>(i)});
    }
    if (!use_reference && context.state.block_count > 0) {
        int first = 0;
        for (int i = 0; i < context.state.block_count; ++i) {
            if (i + 1 == context.state.block_count ||
                context.state.cblock[i + 1].iscommercial != context.state.cblock[first].iscommercial) {
                chapters.push_back({{time(context.state.cblock[first].f_start), time(context.state.cblock[i].f_end)},
                                    context.state.cblock[first].iscommercial != 0});
                first = i + 1;
            }
        }
    } else {
        long first = 0;
        for (int i = 0; i <= count; ++i) {
            const long start = list[i].start_frame, end = list[i].end_frame;
            if (first < start) chapters.push_back({{time(first), time(start)}, false});
            chapters.push_back({{time(start), time(end)}, true});
            first = end;
        }
        if (first < context.state.frame_count - 1)
            chapters.push_back({{time(first), time(context.state.frame_count - 1)}, false});
    }
    MkvOptions mkv; mkv.ordered_without_commercials = context.settings.output_mkvtoolnix == 2;
    if (context.settings.output_mkvtoolnix > 0) {
        mkv.commercial_label = context.translator.text("output_commercial");
        mkv.show_label = context.translator.text("output_show");
        mkv.with_commercials_title = context.translator.text("output_with_commercials");
        mkv.without_commercials_title = context.translator.text("output_without_commercials");
    }
    std::vector<std::pair<std::filesystem::path, std::string>> documents;
    const auto write = [&](const char* extension, auto serializer) {
        std::ostringstream output; serializer(output);
        auto filename = path(context.state.outbasename); filename += extension;
        documents.emplace_back(std::move(filename), output.str());
    };
    if (context.settings.output_videoredo3) write(".VPrj", [&](auto& o) { write_videoredo3(o, media, cuts, scenes); });
    if (context.settings.output_edlx) write(".edlx", [&](auto& o) { write_edlx(o, bytes); });
    if (context.settings.output_btv) write(".chapters.xml", [&](auto& o) { write_btv(o, btv); });
    if (context.settings.output_cuttermaran) write(".cpf", [&](auto& o) {
        write_cuttermaran(o, media, retained, {context.settings.cuttermaran_options}); });
    if (context.settings.output_dvrmstb) write(".xml", [&](auto& o) { write_dvrmstb(o, dvr); });
    if (context.settings.output_plist_cutlist) write(".plist", [&](auto& o) { write_plist_cutlist(o, plist); });
    if (context.settings.output_mkvtoolnix > 0) write(".mkvtoolnix.chapters", [&](auto& o) { write_mkv_chapters(o, chapters, mkv); });
    if (context.settings.output_mkvtoolnix == 2) write(".mkvtoolnix.tags", [&](auto& o) { write_mkv_tags(o, mkv); });
    // Validate/serialize every requested format before replacing any output.
    for (const auto& [filename, text] : documents) {
        std::ofstream file(filename, std::ios::binary | std::ios::trunc);
        if (!file) comskip::request_exit(6);
        file.exceptions(std::ios::failbit | std::ios::badbit);
        file.write(text.data(), static_cast<std::streamsize>(text.size()));
        file.close();
    }
}
