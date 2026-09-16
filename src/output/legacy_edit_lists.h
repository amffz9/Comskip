#pragma once
#include <cstdint>
#include <iosfwd>
#include <span>
#include <string_view>
namespace comskip::output {
struct WombleClip {
  int number;
  bool commercial;
  std::int64_t start, length;
};
struct MlsBookmark {
  std::int64_t frame;
  bool show;
};
void write_womble(std::ostream &, std::string_view filename,
                  std::span<const WombleClip>);
void write_mls(std::ostream &, std::string_view filename, int declared_count,
               std::span<const MlsBookmark>);
} // namespace comskip::output
