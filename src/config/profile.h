#pragma once
#include "ini.h"
#include <vector>
namespace comskip::config {
struct CommercialProfile {
    std::vector<int> strict_lengths;
    std::vector<int> optional_lengths;
    double correction;
    double minimum_tolerance;
    double maximum_tolerance;
    double show_margin;
};
CommercialProfile read_profile(const Ini&, CommercialProfile base);
}
