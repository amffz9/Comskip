#pragma once

#include <string_view>

namespace comskip::build {

#ifdef DONATOR
inline constexpr std::string_view distribution_variant = "donator";
#else
inline constexpr std::string_view distribution_variant = "public";
#endif

} // namespace comskip::build
