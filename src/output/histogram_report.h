#pragma once
#include <algorithm>
#include <concepts>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace comskip::output {
enum class HistogramReportError { invalid_geometry, negative_count };
struct HistogramRow {
    std::int64_t label{};
    std::uint64_t count{};
    double cumulative_fraction{};
    std::string stars;
};
struct HistogramReport { double divisor{}; std::vector<HistogramRow> rows; };

template <std::integral Count>
[[nodiscard]] std::expected<HistogramReport,HistogramReportError> make_histogram_report(
    std::span<const Count> counts, std::size_t maximum_bins, std::size_t output_bins,
    std::int64_t label_scale, std::size_t columns, std::uint64_t denominator) {
    if (maximum_bins>counts.size() || output_bins>counts.size() || columns==0)
        return std::unexpected(HistogramReportError::invalid_geometry);
    std::uint64_t maximum{};
    for (std::size_t index=0; index<maximum_bins; ++index) {
        if constexpr (std::is_signed_v<Count>)
            if (counts[index]<0) return std::unexpected(HistogramReportError::negative_count);
        maximum=std::max(maximum,static_cast<std::uint64_t>(counts[index]));
    }
    HistogramReport report;
    report.divisor=maximum==0 ? 0.0 : static_cast<double>(columns)/static_cast<double>(maximum);
    report.rows.reserve(output_bins);
    std::uint64_t cumulative{};
    for (std::size_t index=0; index<output_bins; ++index) {
        if constexpr (std::is_signed_v<Count>)
            if (counts[index]<0) return std::unexpected(HistogramReportError::negative_count);
        const auto count=static_cast<std::uint64_t>(counts[index]);
        cumulative+=count;
        const auto star_count=count==0 ? std::size_t{} :
            std::min(columns+1,static_cast<std::size_t>(count*report.divisor)+1);
        report.rows.push_back({static_cast<std::int64_t>(index)*label_scale,count,
            denominator==0 ? 0.0 : static_cast<double>(cumulative)/static_cast<double>(denominator),
            std::string(star_count,'*')});
    }
    return report;
}
}
