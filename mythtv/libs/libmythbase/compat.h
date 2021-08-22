// -*- Mode: c++ -*-
// Simple header which encapsulates platform incompatibilities, so we
// do not need to litter the codebase with ifdefs.

#ifndef COMPAT_H
#define COMPAT_H

#ifdef __cplusplus
#    include <cstdio>         // for snprintf(), used by inline dlerror()
#    include <QtGlobal>       // for Q_OS_XXX
#else
#    include <stdio.h>        // for snprintf(), used by inline dlerror()
#endif

#ifdef _WIN32
#    ifndef _MSC_VER
#        define close wsock_close
#    endif

#    ifndef NOMINMAX
#        define NOMINMAX
#    endif

#    include <windows.h>

#    undef DialogBox
#    undef LoadImage
#    undef LoadIcon
#    undef GetObject
#    undef DrawText
#    undef CreateDialog
#    undef CreateFont
#    undef DeleteFile
#    undef GetCurrentTime
#    undef SetJob
#    undef SendMessage

#    ifndef _MSC_VER
#        include <winsock2.h>
#        include <ws2tcpip.h>
#    else
#        include <io.h>
#    endif

#    undef close
#else
#    include <sys/time.h>     // Mac OS X needs this before sys/resource
#    include <sys/resource.h> // for setpriority
#    include <sys/socket.h>
#    include <sys/wait.h>     // For WIFEXITED on Mac OS X
#endif

#ifdef _MSC_VER
    #include <cstdlib>       // for rand()
    #include <ctime>
    #include <sys/time.h>
#endif
#if defined(USING_MINGW)
    #include <time.h>
#endif

#ifdef _MSC_VER
    // Turn off the visual studio warnings (identifier was truncated)
    #pragma warning(disable:4786)

    #ifdef restrict
    #undef restrict
    #endif

    #include <cinttypes>
    #include <direct.h>
    #include <process.h>

    #define strtoll             _strtoi64
    #define strncasecmp         _strnicmp
    #define snprintf            _snprintf

    #ifdef  _WIN64
        using ssize_t = __int64;
    #else
        using ssize_t = int;
    #endif

    // Check for execute, only checking existance in MSVC
    #define X_OK    0

    #if (_MSC_VER < 1800)
        #define rint( x )               floor(x + 0.5)
        #define round( x )              floor(x + 0.5)

        #if ( _MSC_VER < 1700)
            #define signbit( x )        ( x < 0 )
        #endif

        #undef M_PI
        #define M_PI 3.14159265358979323846
    #endif

    #define getpid()                _getpid()
    #define ftruncate( fd, fsize )  _chsize( fd, fsize )

    #ifndef S_ISCHR
    #   ifdef S_IFCHR
    #       define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
    #   else
    #       define S_ISCHR(m) 0
    #   endif
    #endif /* !S_ISCHR */

    #ifndef S_ISBLK
    #   define S_ISBLK(m) 0
    #endif

    #ifndef S_ISREG
    #   define S_ISREG(m) 1
    #endif

    #ifndef S_ISDIR
    #  ifdef S_IFDIR
    #       define S_ISDIR(m) (((m) & S_IFDIR) == S_IFDIR )
    #   else
    #       define S_ISDIR(m) 0
    #   endif
    #endif

    using mode_t = uint32_t;

    #if !defined(__cplusplus) && !defined( inline )
    #   define inline __inline
    #endif

    #if !defined(__func__) // C99 & C++11
    #   define __func__ __FUNCTION__
    #endif

#endif

#ifdef _WIN32
#    define fsync(FD) 0
//used in videodevice only - that code is not windows-compatible anyway
#    define minor(X) 0

	#if defined(__cplusplus)
            using uint = unsigned int;
        #else
            typedef unsigned int uint;
   #endif
#endif

#if defined(__cplusplus) && defined(_WIN32)
#   include <QtGlobal>

#if QT_VERSION >= QT_VERSION_CHECK(5,10,0) && !defined(USING_MINGW)
    #include <QRandomGenerator>
    static inline void srandom(unsigned int /*seed*/) { }
    static inline long int random(void)
        { return QRandomGenerator::global()->generate64(); }
#else
    // cppcheck-suppress qsrandCalled
    static inline void srandom(unsigned int seed) { qsrand(seed); }
    static inline long int random(void) { return qrand(); }
#endif

#   define setenv(x, y, z) ::SetEnvironmentVariableA(x, y)
#   define unsetenv(x) 0


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
#endif

#ifdef _WIN32
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
#endif // _WIN32

#ifdef _MSC_VER
#   define SIGTRAP    SIGBREAK
#   define STDERR_FILENO (int)GetStdHandle( STD_ERROR_HANDLE )
#endif

#ifdef _WIN32
    //signals: not tested
#    define SIGHUP  1
#    define SIGQUIT 3
#    define SIGKILL 9
#    define SIGUSR1 10  // used to force UPnP mediamap rebuild in the backend
#    define SIGUSR2 12  // used to restart LIRC as required
#    define SIGPIPE 13  // not implemented in MINGW, will produce "unable to ignore sigpipe"
#    define SIGALRM 14
#    define SIGCONT 18
#    define SIGSTOP 19

#    define S_IRGRP 0
#    define S_IROTH 0
#    define O_SYNC 0

    #define mkfifo(path, mode) \
        (int)CreateNamedPipeA(path, PIPE_ACCESS_DUPLEX | WRITE_DAC, \
                          PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, \
                          1024, 1024, 10000, nullptr)

#    define RTLD_LAZY 0
#    define dlopen(x, y) LoadLibraryA((x))
#    define dlclose(x) !FreeLibrary((HMODULE)(x))
#    define dlsym(x, y) GetProcAddress((HMODULE)(x), (y))

#    ifdef __cplusplus
        inline const char *dlerror(void)
        {
          #define DLERR_MAX 512
            static char errStr[DLERR_MAX];
            DWORD       errCode = GetLastError();

            if (!FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM |
                                FORMAT_MESSAGE_IGNORE_INSERTS |
                                FORMAT_MESSAGE_MAX_WIDTH_MASK,
                                nullptr, errCode,
                                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                errStr, DLERR_MAX - 1, nullptr))
                snprintf(errStr, DLERR_MAX - 1,
                         "dlopen()/dlsym() caused error %d", (int)errCode);

            return errStr;
        }
#    else  // __cplusplus
#        define dlerror()  "dlerror() is unimplemented."
#   endif // __cplusplus

    // getuid/geteuid/setuid - not implemented
#   define getuid() 0
#    define geteuid() 0
#    define setuid(x) 0
#    define seteuid(x) 0
#endif // _WIN32

#if _MSC_VER && !defined(gmtime_r)
// FFmpeg libs already have a workaround, use it if the headers are included,
// use this otherwise.
static __inline struct tm *gmtime_r(const time_t *timep, struct tm *result)
{
    // this is safe on windows, where gmtime uses a thread local variable.
    // using _gmtime_s() would be better, but needs to be tested on windows.
    struct tm *tmp = gmtime(timep);
    if (tmp)
    {
        *result = *tmp;
        return result;
    }
    return nullptr;
}
#endif

#if  _MSC_VER && !defined(localtime_r)
// FFmpeg libs already have a workaround, use it if the headers are included,
// use this otherwise.
static __inline struct tm *localtime_r(const time_t *timep, struct tm *result)
{
    // this is safe, windows uses a thread local variable for localtime().
    if (timep && result)
    {
        struct tm *win_tmp = localtime(timep);
        memcpy(result, win_tmp, sizeof(struct tm));
        return result;
    }
    return nullptr;
}
#endif

#ifdef _WIN32
// TODO this stuff is not implemented yet
#    define daemon(x, y) -1
#    define getloadavg(x, y) -1

// this stuff is untested
#    define WIFEXITED(w)   (((w) & 0xff) == 0)
#    define WIFSIGNALED(w) (((w) & 0x7f) > 0 && (((w) & 0x7f) < 0x7f))
#    define WIFSTOPPED(w)  (((w) & 0xff) == 0x7f)
#    define WEXITSTATUS(w) (((w) >> 8) & 0xff)
#    define WTERMSIG(w)    ((w) & 0x7f)

#endif // _WIN32

#include <sys/param.h>  // Defines BSD on FreeBSD, Mac OS X

#include "mythconfig.h"

// Libdvdnav now uses off64_t lseek64(), which BSD/Darwin doesn't have.
// Luckily, its lseek() is already 64bit compatible
#ifdef BSD
    typedef off_t off64_t; //NOLINT(modernize-use-using) included from dvdnav C code
    #define lseek64(f,o,w) lseek(f,o,w)
#endif

#if defined(_MSC_VER)
#include <sys/stat.h>   // S_IREAD/WRITE on MinGW
#  define S_IRUSR _S_IREAD
#  ifndef lseek64
#    define lseek64( f, o, w ) _lseeki64( f, o, w )
#  endif
#endif

#ifdef _WIN32
#    define fseeko(stream, offset, whence) fseeko64(stream, offset, whence)
#    define ftello(stream) ftello64(stream)
#endif

#ifdef Q_OS_ANDROID
#ifndef S_IREAD
#define S_IREAD S_IRUSR
#endif
#ifndef S_IWRITE
#define S_IWRITE S_IRUSR
#endif
#endif

#ifdef _WIN32
#   ifdef LZO_COMPILE_TIME_ASSERT_HEADER
#   undef LZO_COMPILE_TIME_ASSERT_HEADER
#   endif

#   define LZO_COMPILE_TIME_ASSERT_HEADER( a )

#   ifdef LZO_COMPILE_TIME_ASSERT
#   undef LZO_COMPILE_TIME_ASSERT
#   endif

#   define LZO_COMPILE_TIME_ASSERT( a )
#endif

#ifndef O_NONBLOCK
#   define O_NONBLOCK    04000
#endif

#endif // COMPAT_H
