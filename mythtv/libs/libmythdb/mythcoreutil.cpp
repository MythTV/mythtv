
#include "mythcoreutil.h"

// POSIX
#include <unistd.h>
#include <fcntl.h>

// System specific C headers
#include "compat.h"

#ifdef linux
#include <sys/vfs.h>
#include <sys/sysinfo.h>
#endif

#if CONFIG_DARWIN
#include <mach/mach.h>
#endif

#ifdef BSD
#include <sys/mount.h>  // for struct statfs
#include <sys/sysctl.h>
#endif

// Qt headers
#include <QByteArray>

// libmythdb headers
#include "mythverbose.h"
#include "unzip.h"

/** \fn getDiskSpace(const QString&,long long&,long long&)
 *  \brief Returns free space on disk containing file in KiB,
 *          or -1 if it does not succeed.
 *  \param file_on_disk file on the file system we wish to stat.
 */
long long getDiskSpace(const QString &file_on_disk,
                       long long &total, long long &used)
{
    struct statfs statbuf;
    bzero(&statbuf, sizeof(statbuf));
    long long freespace = -1;
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

bool extractZIP(const QString &zipFile, const QString &outDir)
{
    UnZip uz;
    UnZip::ErrorCode ec = uz.openArchive(zipFile);

    if (ec != UnZip::Ok)
    {
        VERBOSE(VB_IMPORTANT,
                QString("extractZIP(): Unable to open ZIP file %1")
                        .arg(zipFile));
        return false;
    }

    ec = uz.extractAll(outDir);
    
    if (ec != UnZip::Ok)
    {
        VERBOSE(VB_IMPORTANT,
                QString("extractZIP(): Error extracting ZIP file %1")
                        .arg(zipFile));
        return false;
    }

    uz.closeArchive();

    return true;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
