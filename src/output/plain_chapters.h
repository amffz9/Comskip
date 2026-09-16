#pragma once
#include <cstdint>
#include <iosfwd>
#include <span>
namespace comskip::output {
struct PlainChapterHeader {
  std::int64_t last_frame;
  int fps_times_100;
};
void write_plain_chapters(std::ostream &, PlainChapterHeader,
                          std::span<const std::int64_t> boundaries);
} // namespace comskip::output
