#include "output/legacy_edit_lists.h"
#include "diagnostic.h"
#include <format>
#include <ostream>
#include <stdexcept>
namespace comskip::output {
void write_womble(std::ostream &out, std::string_view filename,
                  std::span<const WombleClip> clips) {
  for (const auto &clip : clips)
    if (clip.number < 1 || clip.start < 0 || clip.length < 0)
      throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(
          comskip::diagnostics::Code::invalid_legacy_edit_list_record);
  for (const auto &clip : clips)
    out << std::format("CLIPLIST: #{} {}\nCLIP: {}\n6 {} {}\n", clip.number,
                       clip.commercial ? "commercial" : "show", filename,
                       clip.start, clip.length);
  if (!out)
    throw comskip::diagnostics::DiagnosticError<std::runtime_error>(
        comskip::diagnostics::Code::cannot_write_legacy_cutlist_export);
}
void write_mls(std::ostream &out, std::string_view filename, int count,
               std::span<const MlsBookmark> marks) {
  if (count < 0)
    throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(
        comskip::diagnostics::Code::invalid_legacy_edit_list_record);
  for (const auto &mark : marks)
    if (mark.frame < 0)
      throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(
          comskip::diagnostics::Code::invalid_legacy_edit_list_record);
  out << std::format("[BookmarkList]\nPathName= {}\nVideoStreamID= 0\nFormat= "
                     "frame\nCount= {}\n",
                     filename, count);
  for (const auto &mark : marks)
    out << std::format("{:11} {}\n", mark.frame, mark.show ? 1 : 0);
  if (!out)
    throw comskip::diagnostics::DiagnosticError<std::runtime_error>(
        comskip::diagnostics::Code::cannot_write_legacy_cutlist_export);
}
} // namespace comskip::output
