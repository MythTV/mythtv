
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
#include <QStringList>
#include <QFile>

// libmythbase headers
#include "mythcorecontext.h"
#include "mythlogging.h"
#include "unzip.h"

/** \fn getDiskSpace(const QString&,long long&,long long&)
 *  \brief Returns free space on disk containing file in KiB,
 *          or -1 if it does not succeed.
 *  \param file_on_disk file on the file system we wish to stat.
 */
int64_t getDiskSpace(const QString &file_on_disk,
                     int64_t &total, int64_t &used)
{
    struct statfs statbuf;
    memset(&statbuf, 0, sizeof(statbuf));
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

bool extractZIP(const QString &zipFile, const QString &outDir)
{
    UnZip uz;
    UnZip::ErrorCode ec = uz.openArchive(zipFile);

    if (ec != UnZip::Ok)
    {
        LOG(VB_GENERAL, LOG_ERR,
                QString("extractZIP(): Unable to open ZIP file %1")
                        .arg(zipFile));
        return false;
    }

    ec = uz.extractAll(outDir);

    if (ec != UnZip::Ok)
    {
        LOG(VB_GENERAL, LOG_ERR,
                QString("extractZIP(): Error extracting ZIP file %1")
                        .arg(zipFile));
        return false;
    }

    uz.closeArchive();

    return true;
}

bool gzipFile(const QString &inFilename, const QString &gzipFilename)
{
    QFile infile(inFilename);
    QFile outfile(gzipFilename);

    if (!infile.open(QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("gzipFile(): Error opening file for reading '%1'").arg(inFilename));
        return false;
    }

    if (!outfile.open(QIODevice::WriteOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("gzipFile(): Error opening file for writing '%1'").arg(gzipFilename));
        infile.close();
        return false;
    }

    QByteArray uncompressedData = infile.readAll();
    QByteArray compressedData = gzipCompress(uncompressedData);

    if (!outfile.write(compressedData))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("gzipFile(): Error while writing to '%1'").arg(gzipFilename));
        infile.close();
        outfile.close();
        return false;
    }

    infile.close();
    outfile.close();

    return true;
}

bool gunzipFile(const QString &gzipFilename, const QString &outFilename)
{
    QFile infile(gzipFilename);
    QFile outfile(outFilename);

    if (!infile.open(QIODevice::ReadOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("gunzipFile(): Error opening file for reading '%1'").arg(gzipFilename));
        return false;
    }

    if (!outfile.open(QIODevice::WriteOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("gunzipFile(): Error opening file for writing '%1'").arg(outFilename));
        infile.close();
        return false;
    }

    QByteArray compressedData = infile.readAll();
    QByteArray uncompressedData = gzipUncompress(compressedData);

    if (outfile.write(uncompressedData) < uncompressedData.size())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("gunzipFile(): Error while writing to '%1'").arg(outFilename));
        infile.close();
        outfile.close();
        return false;
    }

    infile.close();
    outfile.close();

    return true;
}

QByteArray gzipCompress(const QByteArray& data)
{
    if (data.length() == 0)
        return QByteArray();

    static const int CHUNK_SIZE = 1024;
    char out[CHUNK_SIZE];

    // allocate inflate state
    z_stream strm;

    strm.zalloc   = Z_NULL;
    strm.zfree    = Z_NULL;
    strm.opaque   = Z_NULL;
    strm.avail_in = data.length();
    strm.next_in  = (Bytef*)(data.data());

    int ret = deflateInit2(&strm,
                           Z_DEFAULT_COMPRESSION,
                           Z_DEFLATED,
                           15 + 16,
                           8,
                           Z_DEFAULT_STRATEGY ); // gzip encoding
    if (ret != Z_OK)
        return QByteArray();

    QByteArray result;

    // run deflate()
    do
    {
        strm.avail_out = CHUNK_SIZE;
        strm.next_out  = (Bytef*)(out);

        ret = deflate(&strm, Z_FINISH);

        Q_ASSERT(ret != Z_STREAM_ERROR);  // state not clobbered

        switch (ret)
        {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;     // and fall through
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                (void)deflateEnd(&strm);
                return QByteArray();
        }

        result.append(out, CHUNK_SIZE - strm.avail_out);
    }
    while (strm.avail_out == 0);

    // clean up and return

    deflateEnd(&strm);

    return result;
}

QByteArray gzipUncompress(const QByteArray &data)
{
    if (data.length() == 0)
        return QByteArray();

    static const int CHUNK_SIZE = 1024;
    char out[CHUNK_SIZE];

    // allocate inflate state
    z_stream strm;

    strm.zalloc   = Z_NULL;
    strm.zfree    = Z_NULL;
    strm.opaque   = Z_NULL;
    strm.avail_in = data.length();
    strm.next_in  = (Bytef*)(data.data());

    int ret = inflateInit2(&strm, 15 + 16);

    if (ret != Z_OK)
        return QByteArray();

    QByteArray result;

    do
    {
        strm.avail_out = CHUNK_SIZE;
        strm.next_out = (Bytef*)out;
        ret = inflate(&strm, Z_NO_FLUSH);

        Q_ASSERT(ret != Z_STREAM_ERROR);  // state not clobbered

        switch (ret)
        {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;     // and fall through
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                (void) deflateEnd(&strm);
                return QByteArray();
        }

        result.append(out, CHUNK_SIZE - strm.avail_out);
    }
    while (strm.avail_out == 0);

    (void) inflateEnd(& strm);

    return result;
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
