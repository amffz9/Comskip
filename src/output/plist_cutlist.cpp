#include "output/plist_cutlist.h"

#include <pugixml.hpp>
#include <cmath>
#include <cstdint>
#include <format>
#include <new>
#include <ostream>
#include <stdexcept>
#include <vector>

namespace comskip::output {
namespace {
std::int64_t ticks(Seconds time) {
    const double value = time.count() * 90000.0;
    if (!std::isfinite(time.count()) || time.count() < 0)
        throw std::invalid_argument("plist cutlist time must be finite and nonnegative");
    // Comparing with 2^63 avoids rounding INT64_MAX upward when represented as
    // double. The conversion is defined only after this exclusive bound check.
    if (!std::isfinite(value) || value >= std::ldexp(1.0, 63))
        throw std::out_of_range("plist cutlist time exceeds the integer tick range");
    return static_cast<std::int64_t>(value);
}
pugi::xml_node append(pugi::xml_node parent, const char* name) {
    auto node = parent.append_child(name);
    if (!node) throw std::bad_alloc{};
    return node;
}
void whitespace(pugi::xml_node parent, const char* value) {
    auto node = parent.append_child(pugi::node_pcdata);
    if (!node || !node.set_value(value)) throw std::bad_alloc{};
}
}
void write_plist_cutlist(std::ostream& output, std::span<const TimeInterval> intervals) {
    struct TickInterval { std::int64_t start, end; };
    std::vector<TickInterval> converted;
    converted.reserve(intervals.size());
    for (const auto& interval : intervals) {
        if (interval.end < interval.start)
            throw std::invalid_argument("plist cutlist interval ends before it starts");
        converted.push_back({ticks(interval.start), ticks(interval.end)});
    }
    pugi::xml_document document;
    auto array = append(document, "array");
    whitespace(array, "\n");
    for (const auto& interval : converted) {
        for (int index = 0; index < 2; ++index) {
            const auto value = index == 0 ? interval.start : interval.end;
            auto integer = append(array, "integer");
            if (!integer.text().set(std::format("{}", value).c_str())) throw std::bad_alloc{};
            whitespace(array, index == 0 ? " " : "\n");
        }
    }
    whitespace(document, "\n");
    document.print(output, "", pugi::format_raw, pugi::encoding_utf8);
    if (!output) throw std::ios_base::failure("could not write plist cutlist");
}
}
