#include "saved_logo.h"
#include "config/ini.h"
#include "image_geometry.h"
#include "localization/diagnostic.h"
#include <limits>
#include <string>
#include <format>

namespace comskip::detection {
namespace {
using diagnostics::Code;
template<class T> using Error = diagnostics::DiagnosticError<T>;
bool seek_marker(std::FILE& stream, int marker) {
    int character;
    while ((character = std::getc(&stream)) != EOF) if (character == marker) return true;
    if (std::ferror(&stream)) throw Error<std::runtime_error>(Code::cannot_read_saved_logo);
    return false;
}
int pixel(std::FILE& stream) {
    int character;
    do { character = std::getc(&stream); } while (character == '\n' || character == '\r');
    if (character == EOF) {
        if (std::ferror(&stream)) throw Error<std::runtime_error>(Code::cannot_read_saved_logo);
        throw Error<std::invalid_argument>(Code::truncated_saved_logo_mask);
    }
    return character;
}
int field(const config::Ini& ini, const char* key, int fallback) {
    if (!ini.find(key)) return fallback;
    const double value = ini.number<double>(key);
    if (value < 0 || value >= static_cast<double>(std::numeric_limits<int>::max()) + 1)
        throw Error<std::invalid_argument>(Code::saved_logo_metadata_exceeds_frame_range, {key});
    return static_cast<int>(value); // Preserve representable fractional legacy truncation.
}
}
SavedLogo read_saved_logo(std::FILE& stream, SavedLogoGeometry fallback,
                         LogoScanOptions options, int maximum_width, int maximum_height) {
    std::string metadata, line;
    int character;
    while ((character = std::getc(&stream)) != EOF && character != 0x80 && character != 0x81 && character != 0x82) {
        if (character == '\n') {
            if (line.find('=') != std::string::npos) metadata += line + '\n';
            line.clear();
        } else line += static_cast<char>(character);
    }
    if (std::ferror(&stream)) throw Error<std::runtime_error>(Code::cannot_read_saved_logo);
    if (line.find('=') != std::string::npos) metadata += line + '\n';
    const config::Ini ini(metadata);
    SavedLogoGeometry geometry{field(ini,"picWidth",fallback.width),field(ini,"picHeight",fallback.height),
        field(ini,"logoMinX",fallback.minimum_x),field(ini,"logoMaxX",fallback.maximum_x),
        field(ini,"logoMinY",fallback.minimum_y),field(ini,"logoMaxY",fallback.maximum_y)};
    if (geometry.width > maximum_width || geometry.height > maximum_height)
        throw Error<std::invalid_argument>(Code::saved_logo_dimensions_exceed_supported_limits);
    validate_logo_bounds(geometry.width, geometry.height, geometry.minimum_x, geometry.maximum_x,
        geometry.minimum_y, geometry.maximum_y);
    const auto scan = validate_logo_scan(geometry.width,geometry.height,geometry.width,options);
    if (character != 0x80 && character != 0x82) throw Error<std::invalid_argument>(Code::truncated_saved_logo_mask);
    SavedLogo result{geometry,std::vector<unsigned char>(scan.storage_size),std::vector<unsigned char>(scan.storage_size)};
    auto mask = [&](int kind) {
        for (int y=geometry.minimum_y;y<=geometry.maximum_y;++y)
            for (int x=geometry.minimum_x;x<=geometry.maximum_x;++x) {
                const int value=pixel(stream);
                if (value!=' ' && value!=(kind==0x80?'|':'-') && !(kind==0x82 && (value=='|' || value=='+')))
                    throw Error<std::invalid_argument>(Code::invalid_saved_logo_mask_character);
                const auto index=static_cast<std::size_t>(y)*geometry.width+x;
                if (kind==0x80 || kind==0x82) result.horizontal[index]=(value=='|' || value=='+');
                if (kind==0x81 || kind==0x82) result.vertical[index]=(value=='-' || value=='+');
            }
    };
    // The current writer emits a combined-only mask; older files use two masks.
    if (character==0x82) { mask(0x82); return result; }
    mask(0x80);
    if (!seek_marker(stream,0x81)) throw Error<std::invalid_argument>(Code::truncated_saved_logo_mask);
    mask(0x81);
    if (seek_marker(stream,0x82)) mask(0x82);
    return result;
}
void write_saved_logo(std::FILE& stream, SavedLogoGeometry g,
                      std::span<const unsigned char> horizontal,
                      std::span<const unsigned char> vertical, std::string_view path) {
    validate_logo_bounds(g.width,g.height,g.minimum_x,g.maximum_x,g.minimum_y,g.maximum_y);
    const auto size=checked_image_size(g.width,g.height);
    if (horizontal.size()<size || vertical.size()<size)
        throw Error<std::invalid_argument>(Code::invalid_saved_logo_output_buffers);
    std::string output=std::format("logoMinX={}\nlogoMaxX={}\nlogoMinY={}\nlogoMaxY={}\npicWidth={}\npicHeight={}\n\nCombined Logo Mask\n\202\n",
        g.minimum_x,g.maximum_x,g.minimum_y,g.maximum_y,g.width,g.height);
    for (int y=g.minimum_y;y<=g.maximum_y;++y) {
        for (int x=g.minimum_x;x<=g.maximum_x;++x) {
            const auto index=static_cast<std::size_t>(y)*g.width+x;
            output.push_back(horizontal[index] ? (vertical[index] ? '+' : '|') :
                             (vertical[index] ? '-' : ' '));
        }
        output.push_back('\n');
    }
    if (std::fwrite(output.data(),1,output.size(),&stream)!=output.size())
        throw Error<std::runtime_error>(Code::output_write,{std::string(path)});
}
void write_saved_logo(std::FILE& stream, const SavedLogo& logo, std::string_view path) {
    write_saved_logo(stream,logo.geometry,logo.horizontal,logo.vertical,path);
}
}
