#include "fingerprint/process_usage.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <fstream>
#include <string>
#endif

namespace comskip::fingerprint {
#ifdef _WIN32
namespace {
double seconds(const FILETIME& time) {
    return static_cast<double>((static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime) / 1e7;
}
}

ProcessUsage process_usage() {
    ProcessUsage usage;
    const auto process = GetCurrentProcess();
    PROCESS_MEMORY_COUNTERS memory{};
    if (GetProcessMemoryInfo(process, &memory, sizeof memory)) {
        usage.rss_bytes = memory.WorkingSetSize;
        usage.peak_rss_bytes = memory.PeakWorkingSetSize;
    }
    FILETIME created, exited, kernel, user;
    if (GetProcessTimes(process, &created, &exited, &kernel, &user)) usage.cpu_seconds = seconds(kernel) + seconds(user);
    return usage;
}
#else
ProcessUsage process_usage() {
    ProcessUsage usage;
    rusage self{};
    if (getrusage(RUSAGE_SELF, &self) == 0) {
        usage.cpu_seconds = static_cast<double>(self.ru_utime.tv_sec + self.ru_stime.tv_sec) +
                            static_cast<double>(self.ru_utime.tv_usec + self.ru_stime.tv_usec) / 1e6;
#ifdef __APPLE__
        usage.peak_rss_bytes = static_cast<std::uint64_t>(self.ru_maxrss);  // bytes on macOS
#else
        usage.peak_rss_bytes = static_cast<std::uint64_t>(self.ru_maxrss) * 1024;  // kilobytes
#endif
    }
    // Linux gives resident memory now (VmRSS) and its peak (VmHWM) in kB.
    std::ifstream status("/proc/self/status");
    for (std::string line; std::getline(status, line);) {
        const auto value = [&] { return std::stoull(line.substr(line.find_first_of("0123456789"))) * 1024; };
        if (line.starts_with("VmRSS:")) usage.rss_bytes = value();
        else if (line.starts_with("VmHWM:")) usage.peak_rss_bytes = value();
    }
    return usage;
}
#endif
}
