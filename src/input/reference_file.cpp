#include "input/reference_file.h"
#include "input/checked_number.h"
#include <array>
#include <stdexcept>
#include <string_view>

namespace comskip::input {
std::optional<std::string> read_text_line(std::istream& source, std::size_t maximum) {
    std::string line;
    char character{};
    while (source.get(character)) {
        if (character == '\n') return line;
        if (character == '\0') throw std::invalid_argument("Null character in text input");
        if (line.size() == maximum) throw std::length_error("Text input line exceeds its limit");
        line.push_back(character);
    }
    if (source.bad() || (!source.eof() && source.fail()))
        throw std::runtime_error("Cannot read text input");
    if (line.empty()) return std::nullopt;
    return line;
}
namespace {
std::string_view take_word(std::string_view& text) {
    text = trim_ascii(text);
    const auto end = text.find_first_of(" \t\r\n\f\v");
    const auto word = text.substr(0, end);
    text = end == std::string_view::npos ? std::string_view{} : text.substr(end);
    return word;
}
}
ReferenceFile read_reference_file(std::istream& source, std::size_t maximum_intervals) {
    const auto header = read_text_line(source);
    if (!header) throw std::invalid_argument("Reference input has no header");
    std::string_view remaining = *header;
    for (const auto expected : {"FILE", "PROCESSING", "COMPLETE"})
        if (take_word(remaining) != expected) throw std::invalid_argument("Invalid reference header");
    ReferenceFile result;
    result.declared_frames = parse_number<int>(take_word(remaining), "reference frame count");
    if (result.declared_frames < 0 || take_word(remaining) != "FRAMES" || take_word(remaining) != "AT")
        throw std::invalid_argument("Invalid reference frame count header");
    double rate = parse_number<double>(take_word(remaining), "reference frame rate") / 100;
    if (!trim_ascii(remaining).empty() || rate <= 0)
        throw std::invalid_argument("Invalid reference frame rate header");
    if (rate > 99) rate /= 10; // Historical millisecond precision encoding.
    result.frames_per_second = rate;
    const auto separator = read_text_line(source);
    if (!separator || trim_ascii(*separator).empty() || trim_ascii(*separator).find_first_not_of('-') != std::string_view::npos)
        throw std::invalid_argument("Reference input has no separator");
    while (const auto line = read_text_line(source)) {
        std::string_view fields = trim_ascii(*line);
        if (fields.empty()) break; // Preserve legacy blank-line termination.
        if (result.intervals.size() == maximum_intervals)
            throw std::length_error("Reference interval count exceeds its limit");
        ReferenceInterval interval{parse_number<int>(take_word(fields), "reference start frame"),
                                   parse_number<int>(take_word(fields), "reference end frame")};
        if (interval.start_frame < 0 || interval.end_frame < 0 || !trim_ascii(fields).empty())
            throw std::invalid_argument("Invalid reference frame interval");
        result.intervals.push_back(interval);
    }
    return result;
}
}
