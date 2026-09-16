#include "../localization/diagnostic.h"
#ifndef COMSKIP_CONFIG_INI_H
#define COMSKIP_CONFIG_INI_H
#include <charconv>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace comskip::config {
class Ini {
    std::map<std::string, std::string, std::less<>> values_;
public:
    explicit Ini(std::string_view text = {});
    const std::string* find(std::string_view key) const;
    const auto& values() const { return values_; }
    std::string serialize() const;
    template<class T> T number(std::string_view key) const {
        const auto* value = find(key);
        if (!value) throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::missing_setting, {std::string(key)});
        double result{};
        auto parsed = std::from_chars(value->data(), value->data() + value->size(), result);
        if (parsed.ec != std::errc{} || parsed.ptr != value->data() + value->size() || !std::isfinite(result))
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_number, {std::string(key)});
        if constexpr (std::is_same_v<T, bool>) {
            if (result != 0 && result != 1)
                throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::boolean, {std::string(key)});
        } else if constexpr (std::is_integral_v<T>) {
            if (result < static_cast<double>(std::numeric_limits<T>::lowest()) ||
                result > static_cast<double>(std::numeric_limits<T>::max()) || std::trunc(result) != result)
                throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::integer_range, {std::string(key)});
        }
        return static_cast<T>(result);
    }
};
const Ini& defaults();
}
#endif
