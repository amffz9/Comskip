#include "settings_value.h"
#include "settings_descriptors.h"
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
            throw std::invalid_argument(std::string(key) + " accepts only %s and %%");
        if (title[i] == 's' && ++placeholders > maximum)
            throw std::invalid_argument(std::string(key) + " has too many filename placeholders");
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
                            throw std::invalid_argument("Audio delay out of range");
                        value = -value;
                    }
                }
                target = value;
            }
        }
        if constexpr (std::is_same_v<T, std::string>) {
            // Retain the legacy boundary until every C-string consumer is migrated.
            if (key != "language" && key != "locale_directory" && target.size() >= 1024)
                throw std::invalid_argument(std::string(key) + " is too long");
            if (target.find('\0') != std::string::npos)
                throw std::invalid_argument(std::string(key) + " contains a null character");
        } else {
            if constexpr (std::is_floating_point_v<T>)
                if (!std::isfinite(target))
                    throw std::invalid_argument(std::string(key) + " must be finite");
            if ((key == "thread_count" || key == "num_logo_buffers" ||
                 key == "fps" || key == "edge_radius" || key == "edge_step") && target <= 0)
                throw std::invalid_argument(std::string(key) + " must be positive");
            if (key == "border" && target < 0)
                throw std::invalid_argument("border must be nonnegative");
        }
    });
    if (base.language != "en" && base.language != "es")
        throw std::invalid_argument("Unsupported language: " + base.language);
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
        throw std::invalid_argument("Invalid commercial-length profile");
    return base;
}
}
