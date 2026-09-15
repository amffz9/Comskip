#pragma once

#include <string>
#include <string_view>

namespace comskip::output {
// Escapes a filename for XML element text. Percent encoding preserves the
// historical VideoReDo representation; UTF-8 bytes are retained unchanged.
std::string escape_xml_filename(std::string_view filename);
}
