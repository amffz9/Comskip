#include "../localization/diagnostic.h"
#include "input/frame_record.h"
#include "input/checked_number.h"
#include "input/reference_file.h"
#include <rapidcsv.h>
#include <sstream>
#include <stdexcept>
#include <string>

namespace comskip::input {
namespace {
std::vector<std::string> csv_fields(std::string_view line) {
    if (line.size() > maximum_text_line) throw comskip::diagnostics::DiagnosticError<std::length_error>(comskip::diagnostics::Code::csv_record_exceeds_its_limit);
    if (line.find('\0') != std::string_view::npos) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::null_character_in_csv_record);
    const auto delimiter = line.find(',') == std::string_view::npos ? ';' : ',';
    std::istringstream source{std::string(line)};
    rapidcsv::Document document(source, rapidcsv::LabelParams(-1, -1), rapidcsv::SeparatorParams(delimiter));
    if (document.GetRowCount() != 1) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::expected_one_csv_observation);
    auto fields = document.GetRow<std::string>(0);
    if (!fields.empty() && trim_ascii(fields.back()).empty()) fields.pop_back(); // Historical trailing delimiter.
    return fields;
}
}
std::optional<double> parse_frame_rate(std::string_view header) {
    auto fields = csv_fields(header);
    if (fields.size() < 11 || trim_ascii(fields.front()) != "frame")
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_csv_column_header);
    const auto last = trim_ascii(fields.back());
    if (last.empty()) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::missing_csv_frame_rate);
    if (last.find_first_of("0123456789+-.") != 0) {
        if (fields.size() == 18 && trim_ascii(fields[16]) == "PTS")
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_csv_frame_rate);
        return std::nullopt;
    }
    double rate = parse_number<double>(last, "CSV frame rate");
    if (last.find('.') == std::string_view::npos) {
        rate /= 100;
        if (rate > 99) rate /= 10;
        rate *= 1.00000000000001; // Legacy frame-time roundoff compensation.
    }
    if (rate <= 0) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::csv_frame_rate_must_be_positive);
    return rate;
}
FrameRecord parse_frame_record(std::string_view line) {
    auto fields = csv_fields(line);
    if (fields.size() < 11 || fields.size() > 265) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_csv_observation_column_count);
    const auto integer = [&](std::size_t column, std::string_view name, int fallback = 0) {
        return column < fields.size() ? parse_number<int>(fields[column], name) : fallback;
    };
    const auto real = [&](std::size_t column, std::string_view name) {
        return parse_number<double>(fields.at(column), name);
    };
    FrameRecord result;
    result.number = integer(0, "frame");
    if (result.number <= 0) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::csv_frame_number_must_be_positive);
    result.brightness = integer(1, "brightness"); result.scene_change = integer(2, "scene change") / 5;
    result.logo = integer(3, "logo"); result.uniform = integer(4, "uniform"); result.volume = integer(5, "volume");
    result.min_y = integer(6, "minimum Y"); result.max_y = integer(7, "maximum Y");
    result.aspect_ratio = real(8, "aspect ratio"); result.good_edge = real(9, "good edge");
    if (fields[8].find('.') == std::string::npos) result.aspect_ratio /= 100;
    if (fields[9].find('.') == std::string::npos) result.good_edge /= 500;
    result.black = integer(10, "black flags"); result.cutscene_match = integer(11, "cutscene match");
    result.min_x = integer(12, "minimum X"); result.max_x = integer(13, "maximum X");
    result.bright_count = integer(14, "bright count"); result.dim_count = integer(15, "dim count");
    if (fields.size() > 16) {
        result.timestamp = real(16, "timestamp");
        if (*result.timestamp < 0) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::csv_timestamp_must_be_nonnegative);
    }
    result.segment = integer(17, "segment"); result.audio_channels = integer(18, "audio channels", 2);
    return result;
}
}
