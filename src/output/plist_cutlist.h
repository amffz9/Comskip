#pragma once

#include "output/xml_cutlists.h"

namespace comskip::output {
// The historical cutlist is an XML array fragment, without a plist wrapper or
// declaration. Each interval becomes two integers in 90,000 ticks per second.
// Fractional ticks truncate toward zero. Invalid input is rejected before any
// output; failed streams throw ios_base::failure. Stream formatting is untouched.
void write_plist_cutlist(std::ostream&, std::span<const TimeInterval>);
}
