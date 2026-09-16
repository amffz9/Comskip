#pragma once
#include "logo_geometry.h"
#include <cstdio>
#include <vector>
namespace comskip::detection {
struct SavedLogoGeometry { int width, height, minimum_x, maximum_x, minimum_y, maximum_y; };
struct SavedLogo { SavedLogoGeometry geometry; std::vector<unsigned char> horizontal, vertical; };
// Borrow one already-owned stream; all parsing and validation precede publication.
SavedLogo read_saved_logo(std::FILE& stream, SavedLogoGeometry fallback,
                         LogoScanOptions options, int maximum_width, int maximum_height);
}
