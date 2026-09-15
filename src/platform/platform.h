#ifndef _PLATFORM_H
#define _PLATFORM_H

#ifndef _WIN32
#define _BSD_SOURCE
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
#include <windows.h>        // needed for sleep command
#include <direct.h>         // needed for getcwd
#include <process.h>
#include <io.h>
#include <locale.h>
#include <excpt.h>
#include <winbase.h>

#endif

#ifdef _WIN32
#if defined(_WIN32) && !defined(__MINGW32__) && !defined(__MINGW64__)
#ifndef MAX_PATH
#define MAX_PATH _MAX_PATH
#endif
#elif !defined(MAX_PATH) // MSVC
#define MAX_PATH FILENAME_MAX
#endif // MinGW32,64
#elif  __unix__ // Linux
#define MAX_PATH _POSIX_PATH_MAX
#elif __APPLE__ // MacOSX
#define MAX_PATH PATH_MAX
#else
#error "MAX_PATH is undefined"
#endif

#ifdef _POSIX_ARG_MAX
#define MAX_ARG _POSIX_ARG_MAX
#elif defined(ARG_MAX)
#define MAX_ARG ARG_MAX
#else
#define MAX_ARG MAX_PATH
#endif

#ifndef __cplusplus
#define bool  int
#define false 0
#define true  1
#endif

#include <stdint.h>
#ifdef __cplusplus
#include "portable_threads.h"
#endif

#if defined(__MINGW32__) || defined(__MINGW64__)
typedef FILE* fileh;
typedef struct _stati64* stath;
#elif defined(_WIN32)
typedef FILE* fileh;
typedef struct _stati64* stath;
#else
typedef FILE* fileh;
typedef struct stat* stath;
#endif

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

#ifdef __cplusplus
extern "C" {
#endif
int mystat(char * f, stath s);
fileh myfopen(const char * f, const char * m);
int myremove(char * f);
#ifdef __cplusplus
}
#endif

#ifndef _WIN32
#define _read read
#define _write write
#define _close close
#define _cprintf printf
#define _flushall() fflush(NULL)
#define _getcwd(x, y) getcwd(x, y)
#define Sleep(x) usleep((x)*1000L)

char *_strupr(char *string);
#endif

#if defined(_WIN32) && !defined(__MINGW32__) && !defined(__MINGW64__)
#include <sys/timeb.h>


void gettimeofday (struct timeval * tp, void * dummy);
#endif

#endif

#if defined(__cplusplus) || !defined(_WIN32)
int min(int i, int j);
int max(int i, int j);
#endif
