#include "platform.h"

#include <chrono>
#include <filesystem>
#include <new>
#include <string>
#include <system_error>
#include <thread>

namespace {
std::filesystem::path utf8_path(const char* filename)
{
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(filename)));
}

int file_error(const std::error_code& error)
{
    errno = error.default_error_condition().value();
    return -1;
}
}

FILE* myfopen(const char* filename, const char* mode)
{
    if (!filename || !mode) {
        errno = EINVAL;
        return nullptr;
    }
#if defined(_WIN32)
    try {
        const auto path = utf8_path(filename);
        const std::wstring wide_mode(mode, mode + strlen(mode));
        FILE* stream = nullptr;
        if (_wfopen_s(&stream, path.c_str(), wide_mode.c_str()) != 0)
            return nullptr;
        return stream;
    } catch (const std::filesystem::filesystem_error& error) {
        file_error(error.code());
    } catch (const std::bad_alloc&) {
        errno = ENOMEM;
    } catch (const std::exception&) {
        errno = EILSEQ;
    }
    return nullptr;
#else
    return fopen(filename, mode);
#endif
}

int myremove(const char* filename)
{
    if (!filename) {
        errno = EINVAL;
        return -1;
    }
    try {
        std::error_code error;
        const bool removed = std::filesystem::remove(utf8_path(filename), error);
        if (error) return file_error(error);
        if (!removed) {
            errno = ENOENT;
            return -1;
        }
        return 0;
    } catch (const std::filesystem::filesystem_error& error) {
        return file_error(error.code());
    } catch (const std::bad_alloc&) {
        errno = ENOMEM;
    } catch (const std::exception&) {
        errno = EILSEQ;
    }
    return -1;
}

void sleep_for_ms(long milliseconds)
{
    if (milliseconds > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

namespace comskip::platform {
bool local_time(std::time_t value, std::tm& result) noexcept
{
#if defined(_WIN32)
    return ::localtime_s(&result, &value) == 0;
#else
    return ::localtime_r(&value, &result) != nullptr;
#endif
}
}

#if defined(_WIN32) && !defined(__MINGW32__) && !defined(__MINGW64__)
void gettimeofday(struct timeval* time, void*)
{
    const auto elapsed = std::chrono::system_clock::now().time_since_epoch();
    const auto seconds = std::chrono::floor<std::chrono::seconds>(elapsed);
    time->tv_sec = static_cast<long>(seconds.count());
    time->tv_usec = static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(elapsed - seconds).count());
}
#endif
