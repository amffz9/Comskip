#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <string_view>

// The tool's JSON documents (docs/fingerprint-plan.md, "Cross-repo formats"), read
// and written with nlohmann/json. Objects keep their keys in the order written.
namespace comskip::fingerprint::json {
using Value = nlohmann::ordered_json;

// A JSON document the tool cannot use: malformed, or missing a field it needs.
struct Invalid : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Parses one JSON text (RFC 8259); throws Invalid.
Value parse(std::string_view text);
// Reads and parses a UTF-8 file; throws Invalid when it can't be read.
Value parse_file(const std::string& path);

// The document as compact JSON text; a string that is not valid UTF-8 has its
// bad bytes replaced rather than failing the write.
std::string write(const Value& document);

// The value rounded to `decimals` places, as the formats give their numbers:
// times to 3, scores to 2. Written as the shortest text that reads back as it.
double rounded(double value, int decimals);

// Reads a document's fields: nlohmann's own errors (a missing key, a wrong
// type) come out as Invalid.
template <class Body> auto reading(std::string_view what, Body&& body) {
    try {
        return body();
    } catch (const nlohmann::json::exception& error) {
        throw Invalid(std::string(what) + ": " + error.what());
    }
}
}
