#pragma once
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace comskip::fingerprint::json {
// A JSON document the tool cannot use: malformed, or missing a field it needs.
struct Invalid : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// One parsed JSON value. Numbers are doubles, which holds every count and time
// the formats carry exactly.
class Value {
public:
    using Array = std::vector<Value>;
    using Object = std::map<std::string, Value, std::less<>>;

    Value() = default;
    explicit Value(std::nullptr_t) {}
    explicit Value(bool value) : data_(value) {}
    explicit Value(double value) : data_(value) {}
    explicit Value(std::string value) : data_(std::move(value)) {}
    explicit Value(Array value) : data_(std::make_shared<Array>(std::move(value))) {}
    explicit Value(Object value) : data_(std::make_shared<Object>(std::move(value))) {}

    bool is_null() const { return std::holds_alternative<std::monostate>(data_); }
    bool is_number() const { return std::holds_alternative<double>(data_); }
    bool is_string() const { return std::holds_alternative<std::string>(data_); }
    bool is_object() const { return std::holds_alternative<std::shared_ptr<Object>>(data_); }
    bool is_array() const { return std::holds_alternative<std::shared_ptr<Array>>(data_); }

    // Each throws Invalid naming `what` when the value has another type.
    bool boolean(std::string_view what) const;
    double number(std::string_view what) const;
    const std::string& string(std::string_view what) const;
    const Array& array(std::string_view what) const;
    const Object& object(std::string_view what) const;

    // A member of an object, or nullptr when absent (or this is not an object).
    const Value* find(std::string_view name) const;
    // A member that must be present.
    const Value& at(std::string_view name) const;

private:
    std::variant<std::monostate, bool, double, std::string, std::shared_ptr<Array>, std::shared_ptr<Object>> data_;
};

// Parses one JSON text (RFC 8259); throws Invalid.
Value parse(std::string_view text);
// Reads and parses a UTF-8 file; throws Invalid when it can't be read.
Value parse_file(const std::string& path);

// Writers for the tool's own outputs, which are built as text.
void append_string(std::string& text, std::string_view value);
void append_number(std::string& text, double value, int decimals);
void append_integer(std::string& text, long long value);
}
