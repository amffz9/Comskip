#pragma once
#include "translator.h"
#include <algorithm>
#include <array>

namespace comskip::localization {
inline constexpr auto review_help_message_ids = std::array{
    std::string_view("review_help_dismiss"),
    std::string_view("review_help_header"),
    std::string_view("review_help_arrows"),
    std::string_view("review_help_pages"),
    std::string_view("review_help_half_second"),
    std::string_view("review_help_cutpoint"),
    std::string_view("review_help_block"),
    std::string_view("review_help_zoom"),
    std::string_view("review_help_graph"),
    std::string_view("review_help_xds"),
    std::string_view("review_help_toggle"),
    std::string_view("review_help_write"),
    std::string_view("review_help_cutscene"),
    std::string_view("review_help_volume"),
    std::string_view("review_help_uniformity"),
    std::string_view("review_help_brightness"),
    std::string_view("review_help_timecode"),
    std::string_view("review_help_blank"),
    std::string_view("review_help_commercial"),
    std::string_view("review_help_end"),
    std::string_view("review_help_begin"),
    std::string_view("review_help_insert"),
    std::string_view("review_help_delete"),
    std::string_view("review_help_start"),
    std::string_view("review_help_finish"),
    std::string_view("review_help_blank"),
    std::string_view("review_help_markers"),
    std::string_view("review_help_before"),
    std::string_view("review_help_after"),
    std::string_view("review_help_clear")
};

// The native review adapter accepts a null-terminated array; pointers refer to
// translator-owned strings and remain valid for the translator lifetime.
inline auto review_help(const Translator& translator) {
    std::array<const char*, review_help_message_ids.size() + 1> result{};
    std::ranges::transform(review_help_message_ids, result.begin(),
                           [&](std::string_view id) { return translator.text(id); });
    return result;
}
}
