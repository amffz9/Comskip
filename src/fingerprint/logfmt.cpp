#include "fingerprint/logfmt.h"
#include <algorithm>
#include <format>

namespace comskip::fingerprint::logfmt {
namespace {
bool control(char c) { return static_cast<unsigned char>(c) < 0x20 || c == 0x7f; }
}

Field seconds(std::string_view key, double value) { return {key, std::format("{:.3f}", value)}; }
Field integer(std::string_view key, long long value) { return {key, std::to_string(value)}; }
Field flag(std::string_view key, bool value) { return {key, value ? "1" : "0"}; }

Field text(std::string_view key, std::string_view value) {
    const bool plain = !value.empty() && value.find_first_of(" \"=\\") == std::string_view::npos && std::ranges::none_of(value, control);
    if (plain) return {key, std::string(value)};
    std::string quoted = "\"";
    for (const char c : value) {
        switch (c) {
        case '"': quoted += "\\\""; break;
        case '\\': quoted += "\\\\"; break;
        case '\n': quoted += "\\n"; break;
        case '\r': quoted += "\\r"; break;
        case '\t': quoted += "\\t"; break;
        default:
            if (control(c)) quoted += std::format("\\u{:04x}", static_cast<unsigned char>(c));
            else quoted += c;
        }
    }
    return {key, quoted + "\""};
}

std::string line(std::string_view event, double wall, double now, std::initializer_list<Field> fields) {
    auto out = std::format("event={} wall={:.3f} now={:.3f}", text("event", event).value, wall, now);
    for (const auto& field : fields) out += std::format(" {}={}", field.key, field.value);
    return out + "\n";
}
}
