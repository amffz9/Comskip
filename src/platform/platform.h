#pragma once

#ifndef _WIN32
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#include <unistd.h>
#include <limits.h>
#include <stdarg.h>
#include <ctype.h>
#else
#undef WINVER
#define WINVER _WIN32_WINNT_WIN7
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <memory.h>
#include <time.h>										// needed for play_nice routines
#include <math.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#ifndef _WIN32
#include <sys/time.h>
#endif
#include <inttypes.h>

#ifdef _WIN32
#include <conio.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>        // needed for the legacy Windows review interface
#include <direct.h>         // needed for getcwd
#include <process.h>
#include <io.h>
#include <locale.h>
#include <excpt.h>
#include <winbase.h>

#endif

#include <stdint.h>
#include <ctime>
#include <string>
#ifdef __cplusplus
#include "portable_threads.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif
FILE* myfopen(const char* filename, const char* mode);
int myremove(const char * f);
void sleep_for_ms(long milliseconds);
#ifdef __cplusplus
}
#endif

#if defined(_WIN32) && !defined(__MINGW32__) && !defined(__MINGW64__)
void gettimeofday (struct timeval * tp, void * dummy);
#endif

namespace comskip::platform {
bool local_time(std::time_t value, std::tm& result) noexcept;
std::string time_string(std::time_t value);
}
