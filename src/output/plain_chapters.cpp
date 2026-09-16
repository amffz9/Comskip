#include "output/plain_chapters.h"
#include "diagnostic.h"
#include <format>
#include <ostream>
#include <stdexcept>
namespace comskip::output {
void write_plain_chapters(std::ostream &out, PlainChapterHeader header,
                          std::span<const std::int64_t> boundaries) {
  if (header.last_frame < 0 || header.fps_times_100 <= 0)
    throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(
        comskip::diagnostics::Code::invalid_plain_chapter_record);
  for (auto frame : boundaries)
    if (frame < 0)
      throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(
          comskip::diagnostics::Code::invalid_plain_chapter_record);
  out << std::format(
      "FILE PROCESSING COMPLETE {:6} FRAMES AT {:5}\n-------------------\n",
      header.last_frame, header.fps_times_100);
  for (auto frame : boundaries)
    out << std::format("{}\n", frame);
  if (!out)
    throw comskip::diagnostics::DiagnosticError<std::runtime_error>(
        comskip::diagnostics::Code::cannot_write_legacy_cutlist_export);
}
} // namespace comskip::output
