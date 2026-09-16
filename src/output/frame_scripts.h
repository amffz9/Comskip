#pragma once
#include <cstdint>
#include <iosfwd>
#include <span>
#include <string_view>

namespace comskip::output {
struct VcfRange { std::int64_t start, length; };
struct ScriptFrameRange { std::int64_t start, end; };
void write_vcf(std::ostream& output, std::span<const VcfRange> ranges);
void write_projectx(std::ostream& output, std::span<const ScriptFrameRange> ranges);
// The adapter expands the validated legacy source template. Retained ranges
// are joined according to emitted records, independently of source frame IDs.
void write_avisynth(std::ostream& output, std::string_view source_header,
                    std::span<const ScriptFrameRange> trims);
}
