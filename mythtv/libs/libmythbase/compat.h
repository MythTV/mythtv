// -*- Mode: c++ -*-
// Simple header which encapsulates platform incompatibilities, so we
// do not need to litter the codebase with ifdefs.

#ifndef __COMPAT_H__
#define __COMPAT_H__

// Turn off the visual studio warnings (identifier was truncated)
#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#ifdef _MSC_VER

    #ifdef restrict 
    #undef restrict
    #endif

    #include <inttypes.h>
    #include <direct.h>
    #include <process.h>

    #define strtoll             _strtoi64
    #define strncasecmp         _strnicmp
    #define snprintf            _snprintf

    #ifdef  _WIN64
        typedef __int64    ssize_t;
    #else
        typedef int   ssize_t;
    #endif

    // Check for execute, only checking existance in MSVC
    #define X_OK    0

    #define rint( x )               floor(x + 0.5)
    #define round( x )              floor(x + 0.5)
    #define getpid()                _getpid()
    #define ftruncate( fd, fsize )  _chsize( fd, fsize ) 

    #ifndef S_ISCHR
    #   ifdef S_IFCHR
    #       define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
    #   else
    #       define S_ISCHR(m) 0
    #   endif
    #endif /* !S_ISCHR */

#endif

#ifdef _WIN32
#ifndef _MSC_VER
 #define close wsock_close
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#ifndef _MSC_VER
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <io.h>
#endif
#define setsockopt(a, b, c, d, e) setsockopt(a, b, c, (const char*)(d), e)
#undef close
#include <stdio.h>        // for snprintf(), used by inline dlerror()
#ifndef _MSC_VER
#include <unistd.h>       // for usleep()
#endif
#else
#include <sys/time.h>     // Mac OS X needs this before sys/resource
#include <sys/resource.h> // for setpriority
#include <sys/socket.h>
#include <sys/wait.h>     // For WIFEXITED on Mac OS X
#endif

#ifdef USING_MINGW
#include <unistd.h>       // for usleep()
#include <sys/time.h>
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
#undef CreateDialog
#undef CreateFont
#undef DeleteFile
#endif

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
#define nice(x) ((int)!::SetPriorityClass(\
                    ::GetCurrentProcess(), ((x) < -10) ? \
                        HIGH_PRIORITY_CLASS : (((x) < 0) ? \
                        ABOVE_NORMAL_PRIORITY_CLASS : (((x) > 10) ? \
                        IDLE_PRIORITY_CLASS : (((x) > 0) ? \
                        BELOW_NORMAL_PRIORITY_CLASS : \
                        NORMAL_PRIORITY_CLASS)))))
#define PRIO_PROCESS 0
#define setpriority(x, y, z) ((x) == PRIO_PROCESS && y == 0 ? nice(z) : -1)
#endif // USING_MINGW

#ifdef USING_MINGW
//signals: not tested
#define SIGHUP  1
#define SIGQUIT 3
#define SIGKILL 9
#define SIGUSR1 10  // used to force UPnP mediamap rebuild in the backend
#define SIGUSR2 12  // used to restart LIRC as required
#define SIGPIPE 13  // not implemented in MINGW, will produce "unable to ignore sigpipe"
#define SIGALRM 14
#define SIGCONT 18
#define SIGSTOP 19

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


#ifndef _MSC_VER
#include <sys/param.h>  // Defines BSD on FreeBSD, Mac OS X
#endif
#include <sys/stat.h>   // S_IREAD/WRITE on MinGW, umask() on BSD


// suseconds_t
#include <sys/types.h>

#ifdef USING_MINGW
    typedef long suseconds_t;
#endif

#include "mythconfig.h"
#if CONFIG_DARWIN && ! defined (_SUSECONDS_T)
    typedef int32_t suseconds_t;   // 10.3 or earlier don't have this
#endif

// Libdvdnav now uses off64_t lseek64(), which BSD/Darwin doesn't have.
// Luckily, its lseek() is already 64bit compatible
#ifdef BSD
    typedef off_t off64_t;
    #define lseek64(f,o,w) lseek(f,o,w)
#endif

#if defined(_MSC_VER)
#  define S_IRUSR _S_IREAD
#endif

#ifdef USING_MINGW
#define fseeko(stream, offset, whence) fseeko64(stream, offset, whence)
#define ftello(stream) ftello64(stream)
#endif

#endif // __COMPAT_H__
