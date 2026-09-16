#pragma once
#include <chrono>
#include <iosfwd>
#include <span>

namespace comskip::output {
using SidecarSeconds = std::chrono::duration<double>;
enum class SidecarSegmentKind { show, commercial };
struct SidecarChapter {
    SidecarSeconds start, end;
    SidecarSegmentKind kind;
};
struct SidecarShowSegment {
    SidecarSeconds start, end;
    int number;
};
void write_ffmetadata(std::ostream& output, std::span<const SidecarChapter> chapters);
// Retains the legacy .ffsplit command syntax, including segment indices and
// the trailing space before each newline. No shell commands are executed.
void write_ffsplit(std::ostream& output, std::span<const SidecarShowSegment> segments);
}
