// -*- Mode: c++ -*-
// Simple header which encapsulates platform incompatibilities, so we
// do not need to litter the codebase with ifdefs.

#ifndef __COMPAT_H__
#define __COMPAT_H__

// Turn off the visual studio warnings (identifier was truncated)
#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#ifdef _WIN32
#define close wsock_close
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#define setsockopt(a, b, c, d, e) setsockopt(a, b, c, (const char*)(d), e)
#undef close
#include <stdio.h>        // for snprintf(), used by inline dlerror()
#else
#include <sys/time.h>     // Mac OS X needs this before sys/resource
#include <sys/resource.h> // for setpriority
#include <sys/socket.h>
#include <sys/wait.h>     // For WIFEXITED on Mac OS X
#endif

#ifdef _WIN32
typedef unsigned int uint;
#endif

#ifdef _WIN32
#undef DialogBox
#undef LoadImage
#undef LoadIcon
#undef GetObject
#undef DrawText
#undef CreateFont
#endif

// Dealing with Microsoft min/max mess: 
// assume that under Windows the code is compiled with NOMINMAX defined
// which disables #define's for min/max.
// however, Microsoft  violates the C++ standard even with 
// NOMINMAX on, and defines templates _cpp_min and _cpp_max 
// instead of templates min/max
// define the correct templates here

#if defined(__cplusplus) && defined(_WIN32) && !defined(USING_MINGW)
template<class _Ty> inline
        const _Ty& max(const _Ty& _X, const _Ty& _Y)
        {return (_X < _Y ? _Y : _X); }
template<class _Ty, class _Pr> inline
	const _Ty& max(const _Ty& _X, const _Ty& _Y, _Pr _P)
	{return (_P(_X, _Y) ? _Y : _X); }

template<class _Ty> inline
        const _Ty& min(const _Ty& _X, const _Ty& _Y)
        {return (_Y < _X ? _Y : _X); }
template<class _Ty, class _Pr> inline
	const _Ty& min(const _Ty& _X, const _Ty& _Y, _Pr _P)
	{return (_P(_Y, _X) ? _Y : _X); }
#endif // defined(__cplusplus) && defined(_WIN32)

#ifdef _WIN32
#undef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef USING_MINGW
#define fsync(FD) 0
//used in videodevice only - that code is not windows-compatible anyway
#define minor(X) 0
#endif

#if defined(__cplusplus) && defined(USING_MINGW)
inline int random(void)
{
    srand(GetTickCount());
    return rand() << 20 ^ rand() << 10 ^ rand();
}
#endif // USING_MINGW

#if defined(__cplusplus) && defined(USING_MINGW)
#define setenv(x, y, z) ::SetEnvironmentVariableA(x, y)
#define unsetenv(x) 0
#endif

#if defined(__cplusplus) && defined(USING_MINGW)
/* TODO: most small usleep's in MythTV are just a quick way to perform
 * a yield() call, those should just be replaced with an actual yield().
 * There is a known bug with Win32 yield(), it basically functions as
 * a no-op. Sleep(0) yields, but only to higher priority threads, while
 * Sleep(1), performs an actual yield() to any other thread.
 * See: http://lists.boost.org/Archives/boost/2003/02/44937.php
 */
inline int usleep(unsigned int timeout)
{
    /*
    // windows seems to have 1us-resolution timers,
    // however this produces the same results as Sleep
    HANDLE hTimer = ::CreateWaitableTimer(NULL, TRUE, NULL);
    if (hTimer) {
        LARGE_INTEGER li;
        li.QuadPart = -((int)timeout * 10);
        if (SetWaitableTimer(hTimer, &li, 0, NULL, 0, FALSE)) {
            DWORD res = WaitForSingleObject(hTimer, (timeout / 1000 + 1));
            if (res == WAIT_TIMEOUT || res == WAIT_OBJECT_0)
                return 0;
        }
        CloseHandle(hTimer);
    }
    */
    //fallback
    Sleep(timeout < 1000 ? 1 : (timeout + 500) / 1000);
    return 0;
}
#endif // defined(__cplusplus) && defined(USING_MINGW)

#if defined(__cplusplus) && defined(USING_MINGW)
inline unsigned sleep(unsigned int x)
{
    Sleep(x * 1000);
    return 0;
}
#endif // defined(__cplusplus) && defined(USING_MINGW)

#if defined(__cplusplus) && defined(USING_MINGW)
struct statfs {
//   long    f_type;     /* type of filesystem */
   long    f_bsize;    /* optimal transfer block size */
   long    f_blocks;   /* total data blocks in file system */
//   long    f_bfree;    /* free blocks in fs */
   long    f_bavail;   /* free blocks avail to non-superuser */
//   long    f_files;    /* total file nodes in file system */
//   long    f_ffree;    /* free file nodes in fs */
//   long    f_fsid;     /* file system id */
//   long    f_namelen;  /* maximum length of filenames */
//   long    f_spare[6]; /* spare for later */
};
inline int statfs(const char* path, struct statfs* buffer)
{
    DWORD spc = 0, bps = 0, fc = 0, c = 0;

    if (buffer && GetDiskFreeSpaceA(path, &spc, &bps, &fc, &c))
    {
        buffer->f_bsize = bps;
        buffer->f_blocks = spc * c;
        buffer->f_bavail = spc * fc;
        return 0;
    }

    return -1;
}
#endif // USING_MINGW

#ifdef USING_MINGW
#define lstat stat
#define bzero(x, y) memset((x), 0, (y))
#define nice(x) ((int)!::SetThreadPriority(\
                    ::GetCurrentThread(), ((x) < -10) ? \
                        2 : (((x) < 0) ? \
                        1 : (((x) > 10) ? \
                        2 : (((x) > 0) ? 1 : 0)))))
#define PRIO_PROCESS 0
#define setpriority(x, y, z) ((x) == PRIO_PROCESS && y == 0 ? nice(z) : -1)
#endif // USING_MINGW

#ifdef USING_MINGW
//signals: not tested
#define SIGHUP 1
#define SIGQUIT 1
#define SIGPIPE 3   // not implemented in MINGW, will produce "unable to ignore sigpipe"
#define SIGALRM 13
#define SIGUSR1 16  // used to force UPnP mediamap rebuild in the backend

#define S_IRGRP 0
#define S_IROTH 0
#define O_SYNC 0
#endif // USING_MINGW

#ifdef USING_MINGW
#define mkfifo(path, mode) \
    (int)CreateNamedPipeA(path, PIPE_ACCESS_DUPLEX | WRITE_DAC, \
                          PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, \
                          1024, 1024, 10000, NULL)
#endif // USING_MINGW

#ifdef USING_MINGW
#define RTLD_LAZY 0
#define dlopen(x, y) LoadLibraryA((x))
#define dlclose(x) !FreeLibrary((HMODULE)(x))
#define dlsym(x, y) GetProcAddress((HMODULE)(x), (y))
#ifdef __cplusplus
inline const char *dlerror(void)
{
  #define DLERR_MAX 512
    static char errStr[DLERR_MAX];
    DWORD       errCode = GetLastError();

    if (!FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS |
                        FORMAT_MESSAGE_MAX_WIDTH_MASK,
                        NULL, errCode,
                        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                        errStr, DLERR_MAX - 1, NULL))
        snprintf(errStr, DLERR_MAX - 1,
                 "dlopen()/dlsym() caused error %d", (int)errCode);

    return errStr;
}
#else  // __cplusplus
#define dlerror()  "dlerror() is unimplemented."
#endif // __cplusplus
#endif // USING_MINGW

#ifdef USING_MINGW
// getuid/geteuid/setuid - not implemented
#define getuid() 0
#define geteuid() 0
#define setuid(x)
#endif // USING_MINGW

#ifdef USING_MINGW
#define	timeradd(a, b, result)						      \
  do {									      \
    (result)->tv_sec = (a)->tv_sec + (b)->tv_sec;			      \
    (result)->tv_usec = (a)->tv_usec + (b)->tv_usec;			      \
    if ((result)->tv_usec >= 1000000)					      \
      {									      \
	++(result)->tv_sec;						      \
	(result)->tv_usec -= 1000000;					      \
      }									      \
  } while (0)
#define	timersub(a, b, result)						      \
  do {									      \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;			      \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;			      \
    if ((result)->tv_usec < 0) {					      \
      --(result)->tv_sec;						      \
      (result)->tv_usec += 1000000;					      \
    }									      \
  } while (0)
#endif // USING_MINGW

#ifdef USING_MINGW
// TODO this stuff is not implemented yet
#define daemon(x, y) -1
#define getloadavg(x, y) -1
#endif // USING_MINGW

#ifdef USING_MINGW
// this stuff is untested
#define WIFEXITED(w)	(((w) & 0xff) == 0)
#define WIFSIGNALED(w)	(((w) & 0x7f) > 0 && (((w) & 0x7f) < 0x7f))
#define WIFSTOPPED(w)	(((w) & 0xff) == 0x7f)
#define WEXITSTATUS(w)	(((w) >> 8) & 0xff)
#define WTERMSIG(w)	((w) & 0x7f)
#endif // USING_MINGW


// suseconds_t
#include <sys/types.h>

#ifdef USING_MINGW
    typedef long suseconds_t;
#endif

#include "mythconfig.h"
#if defined(CONFIG_DARWIN) && ! defined (_SUSECONDS_T)
    typedef int32_t suseconds_t;   // 10.3 or earlier don't have this
#endif

// Libdvdnav now uses off64_t lseek64(), which Darwin doesn't have.
// Luckily, its lseek() is already 64bit compatible
#ifdef CONFIG_DARWIN
    typedef off_t off64_t;
    #define lseek64(f,o,w) lseek(f,o,w)
#endif

#ifdef __FreeBSD__
    typedef off_t off64_t;
#endif


#endif // __COMPAT_H__
