#include "../localization/diagnostic.h"
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
        if (character == '\0') throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::null_character_in_text_input);
        if (line.size() == maximum) throw comskip::diagnostics::DiagnosticError<std::length_error>(comskip::diagnostics::Code::text_input_line_exceeds_its_limit);
        line.push_back(character);
    }
    if (source.bad() || (!source.eof() && source.fail()))
        throw comskip::diagnostics::DiagnosticError<std::runtime_error>(comskip::diagnostics::Code::cannot_read_text_input);
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
    if (!header) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::reference_input_has_no_header);
    std::string_view remaining = *header;
    for (const auto expected : {"FILE", "PROCESSING", "COMPLETE"})
        if (take_word(remaining) != expected) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_reference_header);
    ReferenceFile result;
    result.declared_frames = parse_number<int>(take_word(remaining), "reference frame count");
    if (result.declared_frames < 0 || take_word(remaining) != "FRAMES" || take_word(remaining) != "AT")
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_reference_frame_count_header);
    double rate = parse_number<double>(take_word(remaining), "reference frame rate") / 100;
    if (!trim_ascii(remaining).empty() || rate <= 0)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_reference_frame_rate_header);
    if (rate > 99) rate /= 10; // Historical millisecond precision encoding.
    result.frames_per_second = rate;
    const auto separator = read_text_line(source);
    if (!separator || trim_ascii(*separator).empty() || trim_ascii(*separator).find_first_not_of('-') != std::string_view::npos)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::reference_input_has_no_separator);
    while (const auto line = read_text_line(source)) {
        std::string_view fields = trim_ascii(*line);
        if (fields.empty()) break; // Preserve legacy blank-line termination.
        if (result.intervals.size() == maximum_intervals)
            throw comskip::diagnostics::DiagnosticError<std::length_error>(comskip::diagnostics::Code::reference_interval_count_exceeds_its_limit);
        ReferenceInterval interval{parse_number<int>(take_word(fields), "reference start frame"),
                                   parse_number<int>(take_word(fields), "reference end frame")};
        if (interval.start_frame < 0 || interval.end_frame < 0 || !trim_ascii(fields).empty())
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_reference_frame_interval);
        result.intervals.push_back(interval);
    }
    return result;
}
}
