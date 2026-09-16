#include "../localization/diagnostic.h"
#include "ini.h"
#include "legacy_quoted_value.h"
#include "config_defaults.h"
#include <cctype>
#include <SimpleIni.h>
#include <algorithm>
#include <tuple>
#include <vector>

namespace comskip::config {
namespace {
std::string decode_legacy_value(std::string_view value) {
    auto result=detail::decode_legacy_ini_value(value);
    if(result) return std::move(*result);
    using diagnostics::Code;
    Code code=Code::unterminated_quoted_ini_value;
    switch(result.error()) {
    case detail::QuotedValueIssue::unexpected_tail: code=Code::unexpected_text_after_quoted_ini_value; break;
    case detail::QuotedValueIssue::incomplete_escape: code=Code::incomplete_ini_escape; break;
    case detail::QuotedValueIssue::unterminated: break;
    }
    throw diagnostics::DiagnosticError<std::invalid_argument>(code);
}
}
Ini::Ini(std::string_view text) {
    CSimpleIniCaseA parser(true, true, false);
    if (parser.LoadData(text.data(), text.size()) < 0)
        throw comskip::diagnostics::DiagnosticError<std::invalid_argument>(comskip::diagnostics::Code::could_not_parse_ini_data);
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
            throw comskip::diagnostics::DiagnosticError<std::runtime_error>(comskip::diagnostics::Code::could_not_serialize_ini_setting);
    }
    std::string result;
    if (writer.Save(result) < 0) throw comskip::diagnostics::DiagnosticError<std::runtime_error>(comskip::diagnostics::Code::could_not_serialize_ini_settings);
    return result;
}
const Ini& defaults() {
    static const Ini document(default_ini);
    return document;
}
}
