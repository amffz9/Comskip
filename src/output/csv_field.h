#pragma once

#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace comskip::output {
// CSV uses doubled quotes inside a quoted field, including multiline fields.
inline std::string csv_field(std::string_view value) {
    std::ostringstream stream;
    stream << std::quoted(std::string(value), '"', '"');
    return stream.str();
}
}
