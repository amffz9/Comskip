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

fileh myfopen(const char* filename, const char* mode)
{
    if (!filename || !mode) {
        errno = EINVAL;
        return nullptr;
    }
#if defined(_WIN32)
    try {
        const auto path = utf8_path(filename);
        const std::wstring wide_mode(mode, mode + strlen(mode));
        return _wfopen(path.c_str(), wide_mode.c_str());
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

int min(int i, int j)
{
    return i < j ? i : j;
}

int max(int i, int j)
{
    return i > j ? i : j;
}

#if !defined(_WIN32)
char* _strupr(char* string)
{
    if (string) {
        for (char* character = string; *character; ++character)
            *character = static_cast<char>(toupper(static_cast<unsigned char>(*character)));
    }
    return string;
}
#endif

#if defined(_WIN32) && !defined(__MINGW32__) && !defined(__MINGW64__)
void gettimeofday(struct timeval* time, void*)
{
    const auto elapsed = std::chrono::system_clock::now().time_since_epoch();
    const auto seconds = std::chrono::floor<std::chrono::seconds>(elapsed);
    time->tv_sec = static_cast<long>(seconds.count());
    time->tv_usec = static_cast<long>(std::chrono::duration_cast<std::chrono::microseconds>(elapsed - seconds).count());
}
#endif
