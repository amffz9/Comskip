#include "fingerprint/json.h"
#include "platform/utf8_paths.h"
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <sstream>

namespace comskip::fingerprint::json {
Value parse(std::string_view text) {
    try {
        return Value::parse(text);
    } catch (const nlohmann::json::parse_error& error) {
        throw Invalid(std::string("JSON ") + error.what());
    }
}

Value parse_file(const std::string& path) {
    std::ifstream input(platform::path_from_utf8(path), std::ios::binary);
    if (!input) throw Invalid("Cannot read " + path);
    std::ostringstream contents;
    contents << input.rdbuf();
    if (input.bad()) throw Invalid("Cannot read " + path);
    try {
        return parse(contents.str());
    } catch (const Invalid& error) {
        throw Invalid(path + ": " + error.what());
    }
}

std::string write(const Value& document) { return document.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace); }

double rounded(double value, int decimals) {
    if (!std::isfinite(value)) throw std::invalid_argument("A number to write is not finite");
    // The decimal text a fixed-point write gives (exact, ties to even), read back.
    std::array<char, 400> text{};
    const auto written = std::to_chars(text.data(), text.data() + text.size(), value, std::chars_format::fixed, decimals);
    if (written.ec != std::errc{}) throw std::invalid_argument("A number cannot be formatted");
    double result = 0;
    std::from_chars(text.data(), written.ptr, result);
    return result;
}
}
