#include "ini.h"
#include "config_defaults.h"
#include <cctype>

namespace comskip::config {
namespace {
std::string_view trim(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.remove_prefix(1);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.remove_suffix(1);
    return value;
}
std::string decode(std::string_view value) {
    value = trim(value);
    if (value.empty() || value.front() != '"') return std::string(trim(value.substr(0, value.find_first_of(";#"))));
    std::string result;
    for (std::size_t i = 1; i < value.size(); ++i) {
        char ch = value[i];
        if (ch == '"') {
            auto tail = trim(value.substr(i + 1));
            if (!tail.empty() && tail.front() != ';' && tail.front() != '#')
                throw std::invalid_argument("Unexpected text after quoted INI value");
            return result;
        }
        if (ch == '\\') {
            if (++i == value.size()) throw std::invalid_argument("Incomplete INI escape");
            switch (value[i]) {
            case 'n': ch = '\n'; break;
            case 't': ch = '\t'; break;
            case '\\': ch = '\\'; break;
            case '"': ch = '"'; break;
            default: result += '\\'; ch = value[i]; break;
            }
        }
        result += ch;
    }
    throw std::invalid_argument("Unterminated quoted INI value");
}
}
Ini::Ini(std::string_view text) {
    if (text.starts_with("\xef\xbb\xbf")) text.remove_prefix(3);
    while (!text.empty()) {
        auto end = text.find('\n');
        auto line = trim(text.substr(0, end));
        text = end == text.npos ? std::string_view{} : text.substr(end + 1);
        if (line.empty() || line.front() == ';' || line.front() == '#' || line.front() == '[') continue;
        auto equals = line.find('=');
        if (equals == line.npos) throw std::invalid_argument("Expected key=value in INI file");
        auto key = trim(line.substr(0, equals));
        if (key.empty()) throw std::invalid_argument("Empty INI key");
        values_[std::string(key)] = decode(line.substr(equals + 1));
    }
}
const std::string* Ini::find(std::string_view key) const {
    auto item = values_.find(key);
    return item == values_.end() ? nullptr : &item->second;
}
std::string Ini::serialize() const {
    std::string result;
    for (const auto& [key, value] : values_) {
        result += key + "=\"";
        for (char ch : value) {
            if (ch == '\\' || ch == '"') result += '\\';
            if (ch == '\n') result += "\\n";
            else if (ch == '\t') result += "\\t";
            else result += ch;
        }
        result += "\"\n";
    }
    return result;
}
const Ini& defaults() {
    static const Ini document(default_ini);
    return document;
}
}
