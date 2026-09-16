#pragma once
#include <cstddef>
#include <istream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace comskip::input {
struct ReferenceInterval { int start_frame{}, end_frame{}; };
struct ReferenceFile {
    int declared_frames{};
    std::optional<double> frames_per_second;
    std::vector<ReferenceInterval> intervals;
};
inline constexpr std::size_t maximum_text_line = 16 * 1024;
// Reads owned, bounded text; EOF before any character returns nullopt.
std::optional<std::string> read_text_line(std::istream& source,
                                        std::size_t maximum = maximum_text_line);
// Parses Comskip's FILE PROCESSING COMPLETE header, separator, and frame pairs.
// Reversed intervals are retained for the application's existing repair policy.
ReferenceFile read_reference_file(std::istream& source,
    std::size_t maximum_intervals = static_cast<std::size_t>(std::numeric_limits<int>::max()));
}
