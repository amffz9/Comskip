#pragma once
#include <cstdint>

namespace comskip::fingerprint {
// This process's resource use so far, for the live log's usage rows. A figure
// the system does not give is 0.
struct ProcessUsage {
    std::uint64_t rss_bytes{};       // resident memory now
    std::uint64_t peak_rss_bytes{};  // the most resident memory so far
    double cpu_seconds{};            // user plus system time, all threads
};
ProcessUsage process_usage();
}
