#include "mythcoreutil.h"

// POSIX
#include <unistd.h>
#include <fcntl.h>

// System specific C headers
#include "compat.h"
#include <QtGlobal>

#ifdef __linux__
#include <sys/vfs.h>
#include <sys/sysinfo.h>
#endif

#ifdef Q_OS_DARWIN
#include <mach/mach.h>
#endif

#ifdef BSD
#include <sys/mount.h>  // for struct statfs
#include <sys/sysctl.h>
#endif

// Qt headers
#include <QByteArray>
#include <QStringList>
#include <QFile>

// libmythbase headers
#include "mythcorecontext.h"
#include "mythlogging.h"

/** \fn getDiskSpace(const QString&,long long&,long long&)
 *  \brief Returns free space on disk containing file in KiB,
 *          or -1 if it does not succeed.
 *  \param file_on_disk file on the file system we wish to stat.
 */
int64_t getDiskSpace(const QString &file_on_disk,
                     int64_t &total, int64_t &used)
{
    struct statfs statbuf {};
    int64_t freespace = -1;
    QByteArray cstr = file_on_disk.toLocal8Bit();

    total = used = -1;

    // there are cases where statfs will return 0 (good), but f_blocks and
    // others are invalid and set to 0 (such as when an automounted directory
    // is not mounted but still visible because --ghost was used),
    // so check to make sure we can have a total size > 0
    if ((statfs(cstr.constData(), &statbuf) == 0) &&
        (statbuf.f_blocks > 0) &&
        (statbuf.f_bsize > 0))
    {
        total      = statbuf.f_blocks;
        total     *= statbuf.f_bsize;
        total      = total >> 10;

        freespace  = statbuf.f_bavail;
        freespace *= statbuf.f_bsize;
        freespace  = freespace >> 10;

        used       = total - freespace;
    }

    return freespace;
}

static QString downloadRemoteFile(const QString &cmd, const QString &url,
                                  const QString &storageGroup,
                                  const QString &filename)
{
    QStringList strlist(cmd);
    strlist << url;
    strlist << storageGroup;
    strlist << filename;

    bool ok = gCoreContext->SendReceiveStringList(strlist);

    if (!ok || strlist.size() < 2 || strlist[0] != "OK")
    {
        LOG(VB_GENERAL, LOG_ERR,
            "downloadRemoteFile(): " + cmd + " returned ERROR!");
        return QString();
    }

    return strlist[1];
}

QString RemoteDownloadFile(const QString &url,
                           const QString &storageGroup,
                           const QString &filename)
{
    return downloadRemoteFile("DOWNLOAD_FILE", url, storageGroup, filename);
}

QString RemoteDownloadFileNow(const QString &url,
                              const QString &storageGroup,
                              const QString &filename)
{
    return downloadRemoteFile("DOWNLOAD_FILE_NOW", url, storageGroup, filename);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

