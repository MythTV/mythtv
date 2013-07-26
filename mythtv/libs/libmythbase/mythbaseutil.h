#ifndef _MYTH_DB_UTIL_H_
#define _MYTH_DB_UTIL_H_

// POSIX
#include <sys/types.h>  // for fnctl
#include <fcntl.h>      // for fnctl
#include <errno.h>      // for checking errno

// Qt
#include <QString>

// MythTV
#include "mythlogging.h"

#ifdef _WIN32
static inline void setup_pipe(int[2], long[2]) {}
#else
static inline void setup_pipe(int mypipe[2], long myflags[2])
{
    int pipe_ret = pipe(mypipe);
    if (pipe_ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to open pipes" + ENO);
        mypipe[0] = mypipe[1] = -1;
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

#endif // _MYTH_DB_UTIL_H_
