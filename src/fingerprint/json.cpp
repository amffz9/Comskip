#include "fingerprint/json.h"
#include "platform/utf8_paths.h"
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iterator>
#include <sstream>
#include <system_error>

namespace comskip::fingerprint::json {
namespace {
std::string kind_error(std::string_view what, std::string_view kind) {
    return std::string(what) + " is not " + std::string(kind);
}

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    Value document() {
        auto value = parse_value(0);
        skip_space();
        if (at_ < text_.size()) fail("text after the JSON value");
        return value;
    }

private:
    static constexpr int max_depth = 64;

    [[noreturn]] void fail(std::string_view what) const {
        throw Invalid("JSON " + std::string(what) + " at byte " + std::to_string(at_));
    }
    void skip_space() {
        while (at_ < text_.size() && (text_[at_] == ' ' || text_[at_] == '\t' || text_[at_] == '\n' || text_[at_] == '\r'))
            ++at_;
    }
    bool consume(std::string_view word) {
        if (text_.substr(at_, word.size()) != word) return false;
        at_ += word.size();
        return true;
    }
    Value parse_value(int depth) {
        if (depth > max_depth) fail("nested too deeply");
        skip_space();
        if (at_ >= text_.size()) fail("ends early");
        const char next = text_[at_];
        if (next == '{') return parse_object(depth);
        if (next == '[') return parse_array(depth);
        if (next == '"') return Value(parse_string());
        if (consume("true")) return Value(true);
        if (consume("false")) return Value(false);
        if (consume("null")) return Value(nullptr);
        return Value(parse_number());
    }
    Value parse_object(int depth) {
        ++at_;
        Value::Object members;
        skip_space();
        if (at_ < text_.size() && text_[at_] == '}') {
            ++at_;
            return Value(std::move(members));
        }
        while (true) {
            skip_space();
            if (at_ >= text_.size() || text_[at_] != '"') fail("object key expected");
            auto key = parse_string();
            skip_space();
            if (!consume(":")) fail("':' expected");
            members.insert_or_assign(std::move(key), parse_value(depth + 1));
            skip_space();
            if (consume("}")) return Value(std::move(members));
            if (!consume(",")) fail("',' or '}' expected");
        }
    }
    Value parse_array(int depth) {
        ++at_;
        Value::Array items;
        skip_space();
        if (consume("]")) return Value(std::move(items));
        while (true) {
            items.push_back(parse_value(depth + 1));
            skip_space();
            if (consume("]")) return Value(std::move(items));
            if (!consume(",")) fail("',' or ']' expected");
        }
    }
    unsigned hex4() {
        if (at_ + 4 > text_.size()) fail("escape ends early");
        unsigned value = 0;
        const auto result = std::from_chars(text_.data() + at_, text_.data() + at_ + 4, value, 16);
        if (result.ec != std::errc{} || result.ptr != text_.data() + at_ + 4) fail("bad \\u escape");
        at_ += 4;
        return value;
    }
    static void append_utf8(std::string& out, unsigned point) {
        if (point < 0x80) {
            out += static_cast<char>(point);
        } else if (point < 0x800) {
            out += static_cast<char>(0xc0 | (point >> 6));
            out += static_cast<char>(0x80 | (point & 0x3f));
        } else if (point < 0x10000) {
            out += static_cast<char>(0xe0 | (point >> 12));
            out += static_cast<char>(0x80 | ((point >> 6) & 0x3f));
            out += static_cast<char>(0x80 | (point & 0x3f));
        } else {
            out += static_cast<char>(0xf0 | (point >> 18));
            out += static_cast<char>(0x80 | ((point >> 12) & 0x3f));
            out += static_cast<char>(0x80 | ((point >> 6) & 0x3f));
            out += static_cast<char>(0x80 | (point & 0x3f));
        }
    }
    std::string parse_string() {
        ++at_;
        std::string out;
        while (true) {
            if (at_ >= text_.size()) fail("string ends early");
            const char next = text_[at_++];
            if (next == '"') return out;
            if (static_cast<unsigned char>(next) < 0x20) fail("control character in string");
            if (next != '\\') {
                out += next;
                continue;
            }
            if (at_ >= text_.size()) fail("escape ends early");
            switch (text_[at_++]) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                unsigned point = hex4();
                if (point >= 0xd800 && point < 0xdc00 && consume("\\u")) {
                    const unsigned low = hex4();
                    if (low < 0xdc00 || low >= 0xe000) fail("bad surrogate pair");
                    point = 0x10000 + ((point - 0xd800) << 10) + (low - 0xdc00);
                }
                append_utf8(out, point);
                break;
            }
            default: fail("unknown escape");
            }
        }
    }
    double parse_number() {
        const auto begin = at_;
        if (at_ < text_.size() && text_[at_] == '-') ++at_;
        while (at_ < text_.size() && std::string_view("0123456789.eE+-").find(text_[at_]) != std::string_view::npos) ++at_;
        double value = 0;
        const auto result = std::from_chars(text_.data() + begin, text_.data() + at_, value);
        if (begin == at_ || result.ec != std::errc{} || result.ptr != text_.data() + at_ || !std::isfinite(value)) {
            at_ = begin;
            fail("value expected");
        }
        return value;
    }

    std::string_view text_;
    std::size_t at_{};
};
}

bool Value::boolean(std::string_view what) const {
    if (const auto* value = std::get_if<bool>(&data_)) return *value;
    throw Invalid(kind_error(what, "true or false"));
}
double Value::number(std::string_view what) const {
    if (const auto* value = std::get_if<double>(&data_)) return *value;
    throw Invalid(kind_error(what, "a number"));
}
const std::string& Value::string(std::string_view what) const {
    if (const auto* value = std::get_if<std::string>(&data_)) return *value;
    throw Invalid(kind_error(what, "a string"));
}
const Value::Array& Value::array(std::string_view what) const {
    if (const auto* value = std::get_if<std::shared_ptr<Array>>(&data_)) return **value;
    throw Invalid(kind_error(what, "an array"));
}
const Value::Object& Value::object(std::string_view what) const {
    if (const auto* value = std::get_if<std::shared_ptr<Object>>(&data_)) return **value;
    throw Invalid(kind_error(what, "an object"));
}
const Value* Value::find(std::string_view name) const {
    const auto* members = std::get_if<std::shared_ptr<Object>>(&data_);
    if (!members) return nullptr;
    const auto found = (*members)->find(name);
    return found == (*members)->end() ? nullptr : &found->second;
}
const Value& Value::at(std::string_view name) const {
    if (const auto* value = find(name)) return *value;
    throw Invalid("JSON field \"" + std::string(name) + "\" is missing");
}

Value parse(std::string_view text) { return Parser(text).document(); }

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

void append_string(std::string& text, std::string_view value) {
    text += '"';
    for (const char character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if (character == '"' || character == '\\') {
            text += '\\';
            text += character;
        } else if (byte < 0x20) {
            constexpr char hex[] = "0123456789abcdef";
            text += "\\u00";
            text += hex[byte >> 4];
            text += hex[byte & 0xf];
        } else {
            text += character;
        }
    }
    text += '"';
}

void append_number(std::string& text, double value, int decimals) {
    if (!std::isfinite(value)) throw std::invalid_argument("JSON number is not finite");
    std::array<char, 64> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::fixed, decimals);
    if (result.ec != std::errc{}) throw std::invalid_argument("JSON number cannot be formatted");
    std::string_view written(buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data()));
    if (written == "-0" || written.starts_with("-0.") && written.find_first_not_of("-0.") == std::string_view::npos)
        written.remove_prefix(1);
    text += written;
}

void append_integer(std::string& text, long long value) {
    std::array<char, 32> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    text.append(buffer.data(), result.ptr);
}
}
