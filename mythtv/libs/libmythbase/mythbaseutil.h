#ifndef MYTH_BASE_UTIL_H
#define MYTH_BASE_UTIL_H

// POSIX
#include <array>
#include <cerrno>       // for checking errno
#include <fcntl.h>      // for fnctl
#include <sys/types.h>  // for fnctl

// Qt
#include <QString>

// MythTV
#include "mythlogging.h"

using pipe_fd_array = std::array<int,2>;
using pipe_flag_array = std::array<long,2>;

#ifdef _WIN32
static inline void setup_pipe(pipe_fd_array&, pipe_flag_array&) {}
#else
static inline void setup_pipe(pipe_fd_array& mypipe, pipe_flag_array& myflags)
{
    int pipe_ret = pipe(mypipe.data());
    if (pipe_ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to open pipes" + ENO);
        mypipe.fill(-1);
    }
    else
    {
        errno = 0;
        long flags = fcntl(mypipe[0], F_GETFL);
        if (0 == errno)
        {
            int ret = fcntl(mypipe[0], F_SETFL, flags|O_NONBLOCK);
            if (ret < 0)
                LOG(VB_GENERAL, LOG_ERR,
                    QString("Set pipe flags error") + ENO);
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Get pipe flags error") + ENO);
        }

        for (uint i = 0; i < 2; i++)
        {
            errno = 0;
            flags = fcntl(mypipe[i], F_GETFL);
            if (0 == errno)
                myflags[i] = flags;
        }
    }
}
#endif

#endif // MYTH_BASE_UTIL_H
