#include "diagnostic.h"
#include "output/player_exports.h"
#include <cmath>
#include <format>
#include <ostream>
#include <stdexcept>
#include <string>

namespace comskip::output {
namespace {
void time(PlayerSeconds value) {
    if (!std::isfinite(value.count()) || value.count() < 0)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_player_chapter_mark);
    if (value.count() >= std::ldexp(1.0,63))
        throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::player_timestamp_exceeds_integer_range);
}
bool boundary(PlayerBoundary value) {
    return value == PlayerBoundary::commercial_start || value == PlayerBoundary::commercial_end;
}
void marks(std::span<const PlayerChapterMark> records) {
    for (const auto& mark : records) {
        time(mark.at);
        if (!mark.number || !boundary(mark.boundary))
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_player_chapter_mark);
    }
}
void intervals(std::span<const PlayerInterval> records) {
    for (const auto& record : records) {
        time(record.start); time(record.end);
        if (record.end < record.start) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_player_export_interval);
    }
}
void finish(std::ostream& output) {
    if (!output) throw comskip::diagnostics::DiagnosticError<std::runtime_error>(comskip::diagnostics::Code::cannot_write_player_export);
}
std::string ipod_time(PlayerSeconds at) {
    // Preserve legacy unpadded hours and centisecond truncation for iPod files.
    double seconds=at.count();
    const auto hours=static_cast<std::int64_t>(seconds/3600);
    seconds-=static_cast<double>(hours)*3600;
    const auto minutes=static_cast<int>(seconds/60);
    seconds-=minutes*60;
    const auto whole=static_cast<int>(seconds);
    const auto centiseconds=static_cast<int>((seconds-whole)*100);
    return std::format("{}:{:02}:{:02}.{:02}",hours,minutes,whole,centiseconds);
}
}
void write_zoomplayer_chapters(std::ostream& output,std::span<const PlayerChapterMark> records,bool initial_show) {
    marks(records);
    if (initial_show) output<<"AddChapter(1,Show Segment)\n";
    for (const auto& mark : records)
        output<<std::format("AddChapterBySecond({},{})\n",static_cast<std::int64_t>(mark.at.count()),
            mark.boundary==PlayerBoundary::commercial_start ? "Commercial Segment" : "Show Segment");
    finish(output);
}
void write_zoomplayer_cuts(std::ostream& output,std::span<const PlayerInterval> records) {
    intervals(records);
    for (const auto& record : records)
        output<<std::format("JumpSegment(\"From={:.4f}\",\"To={:.4f}\")\n",record.start.count(),record.end.count());
    finish(output);
}
void write_scf(std::ostream& output,std::span<const ScfFrameMark> records,ScfOptions options) {
    if (options.rounded_fps<=0) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::scf_frame_rate_must_be_positive);
    for (const auto& mark : records)
        if (mark.frame<0 || !mark.number || !boundary(mark.boundary))
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_scf_frame_mark);
    for (const auto& mark : records) {
        const auto seconds=mark.frame/options.rounded_fps;
        const auto milliseconds=(mark.frame%options.rounded_fps)*1000/options.rounded_fps;
        output<<std::format("CHAPTER{:02}={:02}:{:02}:{:02}.{:03}\nCHAPTER{:02}NAME={}\n",
            mark.number,seconds/3600,seconds/60%60,seconds%60,milliseconds,mark.number,
            mark.boundary==PlayerBoundary::commercial_start ? "Commercial starts" : "Commercial ends");
    }
    finish(output);
}
void write_ipod_chapters(std::ostream& output,std::span<const PlayerChapterMark> records) {
    marks(records);
    output<<"CHAPTER01=00:00:00.000\nCHAPTER01NAME=1\n";
    for (const auto& mark : records)
        output<<std::format("CHAPTER{:02}={}\nCHAPTER{:02}NAME={}\n",mark.number,ipod_time(mark.at),mark.number,mark.number);
    finish(output);
}
void write_bsplayer(std::ostream& output,std::span<const PlayerInterval> records) {
    intervals(records);
    for (const auto& record : records) {
        if (!std::isfinite(record.start.count()*1000) || !std::isfinite(record.end.count()*1000))
            throw comskip::diagnostics::DiagnosticError<std::out_of_range>(comskip::diagnostics::Code::player_timestamp_exceeds_integer_range);
    }
    for (const auto& record : records)
        output<<std::format("1,{:.0f},{:.0f}\n",record.start.count()*1000,record.end.count()*1000);
    finish(output);
}
}
