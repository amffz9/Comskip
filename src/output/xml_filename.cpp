#include "xml_filename.h"

namespace comskip::output {
std::string escape_xml_filename(std::string_view filename)
{
    std::string escaped;
    escaped.reserve(filename.size());
    for (const char character : filename) {
        switch (character) {
        case '&': escaped += "&amp;"; break;
        case '<': escaped += "&lt;"; break;
        case '>': escaped += "&gt;"; break;
        case '%': escaped += "&#37;"; break;
        default: escaped += character; break;
        }
    }
    return escaped;
}
}
