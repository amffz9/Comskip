#include "profile.h"
#include <sstream>
#include <utility>
namespace comskip::config {
namespace {
CommercialProfile& active() {
    static CommercialProfile profile = read_profile(defaults(), {});
    return profile;
}
}
CommercialProfile read_profile(const Ini& ini, CommercialProfile base) {
    auto lengths = [&](const char* key, std::vector<int>& target) {
        if (const auto* text = ini.find(key)) {
            std::istringstream input(*text);
            std::string item;
            target.clear();
            while (std::getline(input, item, ',')) {
                int value = Ini("value=" + item).number<int>("value");
                if (value <= 0) throw std::invalid_argument(std::string(key) + " must contain positive durations");
                target.push_back(value);
            }
            if (target.empty()) throw std::invalid_argument(std::string(key) + " cannot be empty");
        }
    };
    lengths("commercial_lengths", base.strict_lengths);
    lengths("optional_commercial_lengths", base.optional_lengths);
    auto number = [&](const char* key, double& target) {
        if (ini.find(key)) target = ini.number<double>(key);
    };
    number("commercial_length_correction", base.correction);
    number("commercial_minimum_tolerance", base.minimum_tolerance);
    number("commercial_maximum_tolerance", base.maximum_tolerance);
    number("commercial_show_margin", base.show_margin);
    if (base.minimum_tolerance < 0 || base.maximum_tolerance < base.minimum_tolerance || base.show_margin < 0)
        throw std::invalid_argument("Invalid commercial-length tolerance or show margin");
    return base;
}
const CommercialProfile& commercial_profile() { return active(); }
void set_commercial_profile(CommercialProfile profile) { active() = std::move(profile); }
}
