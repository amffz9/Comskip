#include "../localization/diagnostic.h"
#include "settings_value.h"
#include "settings_descriptors.h"
#include "logo_search_time.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace comskip::config {
namespace {
void validate_string_template(std::string_view title, std::string_view key, std::size_t maximum) {
    std::size_t placeholders = 0;
    for (std::size_t i = 0; i < title.size(); ++i) {
        if (title[i] != '%') continue;
        if (++i == title.size() || (title[i] != '%' && title[i] != 's'))
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::setting_template, {std::string(key)});
        if (title[i] == 's' && ++placeholders > maximum)
            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::setting_placeholders, {std::string(key)});
    }
}
constexpr auto keys() {
    constexpr std::size_t count = std::apply([](const auto&... group) {
        return (group.size() + ...);
    }, detail::setting_fields);
    std::array<std::string_view, count + 6> result{};
    std::size_t i = 0;
    std::apply([&](const auto&... group) {
        ([&] { for (const auto& field : group) result[i++] = field.key; }(), ...);
    }, detail::setting_fields);
    for (auto key : {"commercial_lengths", "optional_commercial_lengths",
                     "commercial_length_correction", "commercial_minimum_tolerance",
                     "commercial_maximum_tolerance", "commercial_show_margin"})
        result[i++] = key;
    return result;
}
inline constexpr auto all_keys = keys();
}
std::span<const std::string_view> configured_setting_keys() noexcept { return all_keys; }

Settings default_settings() { return load_settings(defaults(), Settings{}); }

Settings load_settings(const Ini& ini, Settings base) {
    detail::for_each_setting(base, [&](std::string_view key, auto& target, int sign) {
        using T = std::remove_cvref_t<decltype(target)>;
        if (const auto* text = ini.find(key)) {
            if constexpr (std::is_same_v<T, std::string>) {
                target = *text;
            } else {
                T value = ini.number<T>(key);
                if constexpr (std::is_same_v<T, int>) {
                    if (sign == -1) {
                        if (value == std::numeric_limits<T>::lowest())
                            throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::audio_delay_out_of_range);
                        value = -value;
                    }
                }
                target = value;
            }
        }
        if constexpr (std::is_same_v<T, std::string>) {
            if (target.find('\0') != std::string::npos)
                throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::setting_null, {std::string(key)});
        } else {
            if constexpr (std::is_floating_point_v<T>)
                if (!std::isfinite(target))
                    throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::setting_finite, {std::string(key)});
            if ((key == "thread_count" || key == "num_logo_buffers" ||
                 key == "fps" || key == "edge_radius" || key == "edge_step" || key == "review_font_size") && target <= 0)
                throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::setting_positive, {std::string(key)});
            if (key == "border" && target < 0)
                throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::border_must_be_nonnegative);
            if ((key == "max_brightness" || key == "test_brightness") &&
                (target < 0 || target > 255))
                throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::setting_byte, {std::string(key)});
            if ((key == "ticker_tape" || key == "top_ticker_tape" ||
                 key == "ignore_side" || key == "ignore_left_side" ||
                 key == "ignore_right_side") && target < 0)
                throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::setting_nonnegative, {std::string(key)});
            if ((key == "ticker_tape_percentage" || key == "top_ticker_tape_percentage") &&
                (target < 0 || target > 100))
                throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::setting_percent, {std::string(key)});
        }
    });
    if (base.language != "en" && base.language != "es")
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::language, {std::string(base.language)});
    validate_string_template(base.windowtitle, "windowtitle", 1);
    validate_string_template(base.avisynth_options, "avisynth_options", 1);
    validate_string_template(base.dvrcut_options, "dvrcut_options", 3);
    base.commercial_profile = read_profile(ini, std::move(base.commercial_profile));
    const auto& profile = base.commercial_profile;
    const auto valid_lengths = [](const auto& lengths) {
        return !lengths.empty() && std::ranges::all_of(lengths, [](int value) { return value > 0; });
    };
    if (!valid_lengths(profile.strict_lengths) || !valid_lengths(profile.optional_lengths) ||
        !std::isfinite(profile.correction) || !std::isfinite(profile.minimum_tolerance) ||
        !std::isfinite(profile.maximum_tolerance) || !std::isfinite(profile.show_margin))
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::invalid_commercial_length_profile);
    (void)adjusted_logo_search_seconds(base.giveUpOnLogoSearch, base.added_recording);
    return base;
}
}
