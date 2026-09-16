#pragma once
#include <chrono>
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <span>
#include <string_view>
namespace comskip::output {
using CommandSeconds = std::chrono::duration<double>;
struct MpgtxRange {
  std::optional<CommandSeconds> start, end;
};
struct DvrCutRange {
  CommandSeconds start, end;
};
struct Mpeg2SchnittRange {
  std::int64_t start, end;
};
void write_mpgtx(std::ostream &, std::string_view expanded_header,
                 std::span<const MpgtxRange>);
void write_dvrcut(std::ostream &, std::string_view expanded_header,
                  std::span<const DvrCutRange>);
void write_mpeg2schnitt(std::ostream &, std::string_view expanded_header,
                        std::span<const Mpeg2SchnittRange>);
} // namespace comskip::output
