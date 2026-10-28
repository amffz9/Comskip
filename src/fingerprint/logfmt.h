#pragma once
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>

namespace comskip::fingerprint::logfmt {
// One key=value field of a logfmt line, its value already formatted.
struct Field {
    std::string_view key;
    std::string value;
};
// A time or other fractional number, to 3 decimals.
Field seconds(std::string_view key, double value);
Field integer(std::string_view key, long long value);
Field flag(std::string_view key, bool value);  // 1 or 0
// Text: bare when it is a plain word, otherwise in double quotes with ", \ and
// control characters escaped as JSON escapes them.
Field text(std::string_view key, std::string_view value);

// "event=NAME wall=W now=N key=value ...\n": every line starts with the event,
// the wall second since the job started and the capture second reached.
std::string line(std::string_view event, double wall, double now, std::initializer_list<Field> fields = {});
}
