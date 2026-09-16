#pragma once
#include <string_view>

namespace comskip::output {
void write_run_footer(std::string_view path, std::string_view timestamp);
}
