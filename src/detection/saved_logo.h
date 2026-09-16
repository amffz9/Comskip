#pragma once
#include "logo_geometry.h"
#include <cstdio>
#include <span>
#include <string_view>
#include <vector>
namespace comskip::detection {
struct SavedLogoGeometry { int width, height, minimum_x, maximum_x, minimum_y, maximum_y; };
struct SavedLogo { SavedLogoGeometry geometry; std::vector<unsigned char> horizontal, vertical; };
// Borrow one already-owned stream; all parsing and validation precede publication.
SavedLogo read_saved_logo(std::FILE& stream, SavedLogoGeometry fallback,
                         LogoScanOptions options, int maximum_width, int maximum_height);
void write_saved_logo(std::FILE& stream, const SavedLogo& logo, std::string_view path);
void write_saved_logo(std::FILE& stream, SavedLogoGeometry geometry,
                      std::span<const unsigned char> horizontal,
                      std::span<const unsigned char> vertical, std::string_view path);
}
