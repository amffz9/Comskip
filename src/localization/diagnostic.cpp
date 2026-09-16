#include "diagnostic.h"
#include "localization_catalogs.h"
#include "../config/legacy_quoted_value.h"
#include <SimpleIni.h>
#include <iomanip>
#include <map>
#include <sstream>
#include <array>
#include <format>
#include <algorithm>
#include <tuple>
#include <vector>

namespace comskip::diagnostics {
namespace {
const auto& english_templates() {
    static const auto templates = [] {
        std::map<std::string,std::string,std::less<>> result;
        CSimpleIniCaseA ini(true,true,false);
        const auto source=comskip::localization::english_catalog;
        if (ini.LoadData(source.data(),source.size()) < 0) return result;
        CSimpleIniCaseA::TNamesDepend sections;
        ini.GetAllSections(sections);
        std::vector<std::tuple<int,std::string,std::string>> entries;
        for(const auto& section:sections) {
            const auto* values=ini.GetSection(section.pItem);
            if(!values) continue;
            for(const auto& [key,value]:*values)
                entries.emplace_back(key.nOrder,key.pItem,value);
        }
        std::ranges::sort(entries);
        for(const auto& [order,key,value]:entries) {
            auto text=config::detail::decode_legacy_ini_value(value);
            if(text) result[key]=std::move(*text);
        }
        return result;
    }();
    return templates;
}
}
std::string english_message(const Diagnostic& diagnostic) {
    const auto id=message_id(diagnostic.code);
    const auto& templates=english_templates();
    const auto found=templates.find(id);
    if (found == templates.end()) return std::string(id);
    try { return format_template(found->second,diagnostic.arguments); }
    catch(const std::exception&) { return std::string(id); }
}
std::string format_template(std::string_view text,std::span<const std::string> arguments) {
    if(arguments.size()>8) throw std::length_error("Diagnostic argument limit exceeded");
    switch(arguments.size()) {
    case 0: return std::vformat(text,std::make_format_args());
    case 1: return std::vformat(text,std::make_format_args(arguments[0]));
    case 2: return std::vformat(text,std::make_format_args(arguments[0],arguments[1]));
    case 3: return std::vformat(text,std::make_format_args(arguments[0],arguments[1],arguments[2]));
    case 4: return std::vformat(text,std::make_format_args(arguments[0],arguments[1],arguments[2],arguments[3]));
    case 5: return std::vformat(text,std::make_format_args(arguments[0],arguments[1],arguments[2],arguments[3],arguments[4]));
    case 6: return std::vformat(text,std::make_format_args(arguments[0],arguments[1],arguments[2],arguments[3],arguments[4],arguments[5]));
    case 7: return std::vformat(text,std::make_format_args(arguments[0],arguments[1],arguments[2],arguments[3],arguments[4],arguments[5],arguments[6]));
    case 8: return std::vformat(text,std::make_format_args(arguments[0],arguments[1],arguments[2],arguments[3],arguments[4],arguments[5],arguments[6],arguments[7]));
    }
    throw std::length_error("Diagnostic argument limit exceeded");
}
}
