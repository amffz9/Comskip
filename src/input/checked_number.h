#include "../localization/diagnostic.h"
#pragma once
#include <charconv>
#include <cmath>
#include <concepts>
#include <stdexcept>
#include <string>
#include <string_view>

namespace comskip::input {
inline std::string_view trim_ascii(std::string_view value) noexcept {
    constexpr std::string_view whitespace = " \t\r\n\f\v";
    const auto first = value.find_first_not_of(whitespace);
    if (first == std::string_view::npos) return {};
    const auto last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}
template<class T> requires (std::integral<T> || std::floating_point<T>)
T parse_number(std::string_view value, std::string_view field) {
    value = trim_ascii(value);
    if (!value.empty() && value.front() == '+') {
        value.remove_prefix(1);
        if (!value.empty() && (value.front() == '+' || value.front() == '-'))
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_sign, {std::string(field)});
    }
    if (value.empty()) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::missing_number, {std::string(field)});
    T result{};
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_number, {std::string(field)});
    if constexpr (std::floating_point<T>)
        if (!std::isfinite(result)) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::nonfinite_number, {std::string(field)});
    return result;
}
}
