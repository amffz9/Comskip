#include "output/legacy_cutlist_adapter.h"
#include "checked_format.h"
#include "diagnostic.h"
#include "output/legacy_commands.h"
#include "output/legacy_edit_lists.h"
#include "output/plain_chapters.h"
#include "output/output_file.h"
#include "detection/frame_timestamps.h"
#include "platform/utf8_paths.h"
#include "recording_context.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <sstream>
#include <vector>
void WriteLegacyCutlistFiles(RecordingContext &context, bool use_reference) {
  using namespace comskip::output;
  const auto &s = context.state;
  const auto &o = context.settings;
  if (!o.output_womble && !o.output_mls && !o.output_mpgtx &&
      !o.output_dvrcut && !o.output_mpeg2schnitt && !o.output_chapters)
    return;
  const int count = use_reference ? s.reffer_count : s.commercial_count;
  if (use_reference)
    comskip::detection::validate_intervals(s.reffer, count);
  else
    comskip::detection::validate_intervals(s.commercial, count);
  if (!std::isfinite(o.fps) || o.fps <= 0 ||
      o.fps * 100 > std::numeric_limits<int>::max() || s.frame_count < 2 ||
      s.block_count < 0 ||
      static_cast<std::size_t>(s.block_count) > s.cblock.size() ||
      (!s.frame.empty() &&
       (s.framenum_real < 2 ||
        static_cast<std::size_t>(s.framenum_real) > s.frame.size())))
    throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(
        comskip::diagnostics::Code::invalid_legacy_cutlist_geometry);
  const auto position = [&](long f) {
    const double time =
        s.frame.empty()
            ? static_cast<double>(f) / o.fps
            : s.frame[f <= 0 ? 1 : std::min<long>(f, s.framenum_real - 1)].pts;
    const double result = time * o.fps + 1.5;
    if (!std::isfinite(result) || result < 0 || result >= std::ldexp(1.0, 63))
      throw comskip::diagnostics::DiagnosticError<std::out_of_range>(
          comskip::diagnostics::Code::legacy_cutlist_position_exceeds_range);
    return static_cast<std::int64_t>(result);
  };
  const auto at = [&](long f) {
    return CommandSeconds{get_frame_pts(context, f)};
  };
  std::vector<WombleClip> clips;
  std::vector<MlsBookmark> marks;
  std::vector<MpgtxRange> mpgtx;
  std::vector<DvrCutRange> dvr;
  std::vector<Mpeg2SchnittRange> schnitt;
  std::vector<std::int64_t> chapters;
  int bookmark_count = 0;
  const auto append = [&](int i, long prev, long start, long end, bool last,
                          bool commercial_interval) {
    if (commercial_interval) {
      if (start - prev > o.fps)
        clips.push_back({i + 1, false, position(prev + 1),
                         position(start) - position(prev)});
      clips.push_back(
          {i + 1, true, position(start), position(end) - position(start)});
    } else if (end - prev > 0)
      clips.push_back(
          {i + 1, false, position(prev + 1), position(end) - position(prev)});
    if (i == 0) {
      const auto declared = (static_cast<std::int64_t>(count) + 1) * 2 + 1 -
                            (start < o.fps ? 1 : 0);
      if (declared > std::numeric_limits<int>::max())
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(
            comskip::diagnostics::Code::legacy_cutlist_position_exceeds_range);
      bookmark_count = static_cast<int>(declared);
      if (start >= o.fps)
        marks.push_back({0, true});
    } else
      marks.push_back({position(prev), true});
    if (!last)
      marks.push_back({position(start), false});
    else if (start < end - 5) {
      marks.push_back({position(start), false});
      marks.push_back({position(end), true});
    }
    if (!last && start - prev > 0)
      mpgtx.push_back(
          {prev < o.fps ? std::nullopt : std::optional{at(prev)}, at(start)});
    else if (last && end - prev > 0)
      mpgtx.push_back({at(prev + 1), std::nullopt});
    if (start - prev > static_cast<int>(o.fps))
      dvr.push_back({at(prev), at(start)});
    if (end - start > 1)
      schnitt.push_back({position(start), position(end)});
  };
  long previous = -1;
  for (int i = 0; i <= count; ++i) {
    const auto start =
        use_reference ? s.reffer[i].start_frame : s.commercial[i].start_frame;
    const auto end =
        use_reference ? s.reffer[i].end_frame : s.commercial[i].end_frame;
    if (start < 0 || end < start || start <= previous ||
        start >= s.frame_count || end > s.frame_count)
      throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(
          comskip::diagnostics::Code::invalid_legacy_cutlist_interval);
    append(i, previous, start, end, end >= s.frame_count - 2, true);
    previous = end;
  }
  if (count < 0 || previous < s.frame_count - 2)
    append(count + 1, previous, s.frame_count - 2, s.frame_count - 1, true,
           false);
  for (int i = 0; i < s.block_count; ++i)
    chapters.push_back(s.cblock[i].f_end);
  const auto write = [&](const std::string &name, auto serializer) {
    std::ostringstream data;
    serializer(data);
    write_output_file(name, data.str(), name == s.outbasename + ".chap"
                                            ? std::chrono::milliseconds{50}
                                            : std::chrono::milliseconds{});
  };
  if (o.output_womble)
    write(s.outbasename + ".wme",
          [&](auto &out) { write_womble(out, s.mpegfilename, clips); });
  if (o.output_mls)
    write(s.outbasename + ".mls", [&](auto &out) {
      write_mls(out, s.mpegfilename, bookmark_count, marks);
    });
  if (o.output_mpgtx)
    write(s.outbasename + "_mpgtx.bat", [&](auto &out) {
      write_mpgtx(out,
                  std::format("mpgtx.exe -j -f -o \"{}.clean\" \"{}\" ",
                              s.mpegfilename, s.mpegfilename),
                  mpgtx);
    });
  if (o.output_dvrcut) {
    std::string header;
    if (o.dvrcut_options.empty())
      header = "dvrcut \"%1\" \"%2\" ";
    else
      comskip::checked_format(header, o.dvrcut_options.c_str(),
                              s.inbasename.c_str(), s.inbasename.c_str(),
                              s.inbasename.c_str());
    write(s.outbasename + "_dvrcut.bat",
          [&](auto &out) { write_dvrcut(out, header, dvr); });
  }
  if (o.output_mpeg2schnitt) {
    const auto header =
        o.mpeg2schnitt_options.empty()
            ? std::format("mpeg2schnitt.exe /S /E /R{:5.2f}  /Z \"%2\" \"%1\" ",
                          o.fps)
            : o.mpeg2schnitt_options + " ";
    write(s.inbasename + "_mpeg2schnitt.bat",
          [&](auto &out) { write_mpeg2schnitt(out, header, schnitt); });
  }
  if (o.output_chapters)
    write(s.outbasename + ".chap", [&](auto &out) {
      write_plain_chapters(
          out, {s.frame_count - 1, static_cast<int>(o.fps * 100)}, chapters);
    });
}
