#pragma once
#include <expected>
#include <string>
#include <string_view>

namespace comskip::config::detail {
enum class QuotedValueIssue { unexpected_tail, incomplete_escape, unterminated };
inline std::string_view trim_ini_value(std::string_view value) {
    constexpr std::string_view whitespace=" \t\r\n\f\v";
    const auto first=value.find_first_not_of(whitespace);
    if(first==std::string_view::npos) return {};
    return value.substr(first,value.find_last_not_of(whitespace)-first+1);
}
// Historic quoted-value escapes are a compatibility adapter around SimpleIni.
// Returning errors avoids a diagnostic/catalog initialization dependency cycle.
inline std::expected<std::string,QuotedValueIssue> decode_legacy_ini_value(std::string_view value) {
    value=trim_ini_value(value);
    if(value.empty() || value.front()!='"')
        return std::string(trim_ini_value(value.substr(0,value.find_first_of(";#"))));
    std::string result;
    for(std::size_t i=1;i<value.size();++i) {
        char ch=value[i];
        if(ch=='"') {
            const auto tail=trim_ini_value(value.substr(i+1));
            if(!tail.empty() && tail.front()!=';' && tail.front()!='#')
                return std::unexpected(QuotedValueIssue::unexpected_tail);
            return result;
        }
        if(ch=='\\') {
            if(++i==value.size()) return std::unexpected(QuotedValueIssue::incomplete_escape);
            switch(value[i]) {
            case 'n': ch='\n'; break;
            case 't': ch='\t'; break;
            case '\\': ch='\\'; break;
            case '"': ch='"'; break;
            default: result+='\\'; ch=value[i]; break;
            }
        }
        result+=ch;
    }
    return std::unexpected(QuotedValueIssue::unterminated);
}
}
