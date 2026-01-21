// -*- Mode: c++ -*-
// Simple header which encapsulates platform incompatibilities, so we
// do not need to litter the codebase with ifdefs.

#ifndef COMPAT_H
#define COMPAT_H

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#include <QtSystemDetection>
#endif

#ifdef Q_OS_ANDROID
#   ifndef S_IREAD
#       define S_IREAD S_IRUSR
#   endif
#   ifndef S_IWRITE
#       define S_IWRITE S_IRUSR
#   endif
#endif

#ifndef Q_OS_WINDOWS
#    include <sys/time.h>     // Mac OS X needs this before sys/resource
#    include <sys/resource.h> // for setpriority
#    include <sys/socket.h>
#    include <sys/wait.h>     // For WIFEXITED on Mac OS X
#else // Q_OS_WINDOWS
#        define close wsock_close

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

#        include <winsock2.h>
#        include <ws2tcpip.h>

#    undef close

#    undef CopyFile
#    undef MoveFile

#    define fsync(FD) 0
//used in videodevice only - that code is not windows-compatible anyway
#    define minor(X) 0

            using uint = unsigned int;

#   define setenv(x, y, z) ::SetEnvironmentVariableA(x, y)
#   define unsetenv(x) 0

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

#    define O_SYNC 0

    // Success: mkfifo returns zero but CreateNamedPipeA returns a
    // file handle.  Failure: both return -1.
    #define mkfifo(path, mode) \
        (CreateNamedPipeA(path, PIPE_ACCESS_DUPLEX | WRITE_DAC, \
                          PIPE_WAIT, PIPE_UNLIMITED_INSTANCES,  \
                          1024, 1024, 10000, nullptr) == INVALID_HANDLE_VALUE ? -1 : 0)

#    define RTLD_LAZY 0
#    define dlopen(x, y) LoadLibraryA((x))
#    define dlclose(x) !FreeLibrary((HMODULE)(x))
#    define dlsym(x, y) GetProcAddress((HMODULE)(x), (y))

#       include <cstdio>
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

    // getuid/geteuid/setuid - not implemented
#   define getuid() 0
#    define geteuid() 0
#    define setuid(x) 0
#    define seteuid(x) 0


// TODO this stuff is not implemented yet
#    define daemon(x, y) -1
#    define getloadavg(x, y) -1

// this stuff is untested
#    define WIFEXITED(w)   (((w) & 0xff) == 0)
#    define WIFSIGNALED(w) (((w) & 0x7f) > 0 && (((w) & 0x7f) < 0x7f))
#    define WIFSTOPPED(w)  (((w) & 0xff) == 0x7f)
#    define WEXITSTATUS(w) (((w) >> 8) & 0xff)
#    define WTERMSIG(w)    ((w) & 0x7f)

#endif // Q_OS_WINDOWS

#ifndef O_NONBLOCK
#   define O_NONBLOCK    04000 /* NOLINT(cppcoreguidelines-macro-usage) */
#endif

#endif // COMPAT_H
