#include "ini.h"
#include "config_defaults.h"
#include <cctype>
#include <SimpleIni.h>
#include <algorithm>
#include <tuple>
#include <vector>

namespace comskip::config {
namespace {
std::string_view trim(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.remove_prefix(1);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.remove_suffix(1);
    return value;
}
// Comskip historically permits escaped quoted values. SimpleIni deliberately
// leaves escapes untouched; keep this compatibility adapter separate from INI parsing.
std::string decode_legacy_value(std::string_view value) {
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
    CSimpleIniCaseA parser(true, true, false);
    if (parser.LoadData(text.data(), text.size()) < 0)
        throw std::invalid_argument("Could not parse INI data");
    CSimpleIniCaseA::TNamesDepend sections;
    parser.GetAllSections(sections);
    std::vector<std::tuple<int, std::string, std::string>> entries;
    for (const auto& section : sections) {
        const auto* values = parser.GetSection(section.pItem);
        if (!values) continue;
        for (const auto& [key, value] : *values)
            entries.emplace_back(key.nOrder, key.pItem, value);
    }
    // Legacy settings are flat even when grouped into sections. Apply entries in
    // file order so a later override wins regardless of its section name.
    std::ranges::sort(entries);
    for (const auto& [order, key, value] : entries)
        values_[key] = decode_legacy_value(value);
}
const std::string* Ini::find(std::string_view key) const {
    auto item = values_.find(key);
    return item == values_.end() ? nullptr : &item->second;
}
std::string Ini::serialize() const {
    CSimpleIniCaseA writer(true, false, false);
    for (const auto& [key, value] : values_) {
        std::string quoted = "\"";
        for (char ch : value) {
            if (ch == '\\' || ch == '"') quoted += '\\';
            if (ch == '\n') quoted += "\\n";
            else if (ch == '\t') quoted += "\\t";
            else quoted += ch;
        }
        quoted += '"';
        if (writer.SetValue("", key.c_str(), quoted.c_str()) < 0)
            throw std::runtime_error("Could not serialize INI setting");
    }
    std::string result;
    if (writer.Save(result) < 0) throw std::runtime_error("Could not serialize INI settings");
    return result;
}
const Ini& defaults() {
    static const Ini document(default_ini);
    return document;
}
}
