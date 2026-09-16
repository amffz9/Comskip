#include "output/legacy_commands.h"
#include "diagnostic.h"
#include <cmath>
#include <format>
#include <limits>
#include <ostream>
#include <stdexcept>
namespace comskip::output {
namespace {
void time(CommandSeconds at) {
  if (!std::isfinite(at.count()) ||
      at.count() > std::numeric_limits<int>::max() ||
      at.count() < std::numeric_limits<int>::min())
    throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(
        comskip::diagnostics::Code::invalid_legacy_command_record);
}
std::string hms(CommandSeconds at) {
  int seconds = static_cast<int>(at.count());
  const int hours = seconds / 3600;
  seconds -= hours * 3600;
  const int minutes = seconds / 60;
  seconds -= minutes * 60;
  return std::format("{}:{:02}:{:02}", hours, minutes, seconds);
}
void finish(std::ostream &out) {
  if (!out)
    throw comskip::diagnostics::DiagnosticError<std::runtime_error>(
        comskip::diagnostics::Code::cannot_write_legacy_cutlist_export);
}
} // namespace
void write_mpgtx(std::ostream &out, std::string_view header,
                 std::span<const MpgtxRange> ranges) {
  for (const auto &range : ranges) {
    if (range.start)
      time(*range.start);
    if (range.end)
      time(*range.end);
  }
  out << header;
  for (const auto &range : ranges)
    out << std::format("[{}-{}]{}", range.start ? hms(*range.start) : "",
                       range.end ? hms(*range.end) : "", range.end ? " " : "");
  out << '\n';
  finish(out);
}
void write_dvrcut(std::ostream &out, std::string_view header,
                  std::span<const DvrCutRange> ranges) {
  for (const auto &range : ranges) {
    time(range.start);
    time(range.end);
  }
  out << header;
  for (const auto &range : ranges)
    out << hms(range.start) << ' ' << hms(range.end) << ' ';
  out << '\n';
  finish(out);
}
void write_mpeg2schnitt(std::ostream &out, std::string_view header,
                        std::span<const Mpeg2SchnittRange> ranges) {
  for (const auto &range : ranges)
    if (range.start < 0 || range.end < range.start)
      throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(
          comskip::diagnostics::Code::invalid_legacy_command_record);
  out << header;
  for (const auto &range : ranges)
    out << std::format("/o{} /i{} ", range.start, range.end);
  out << '\n';
  finish(out);
}
} // namespace comskip::output
