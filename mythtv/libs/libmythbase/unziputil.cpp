#include "unziputil.h"

#include <array>

#include "zlib.h"
#undef Z_NULL
#define Z_NULL nullptr

// Qt headers
#include <QByteArray>
#include <QFile>

// libmythbase headers
#include "mythlogging.h"
#include "unzip2.h"

bool extractZIP(QString zipFile, const QString& outDir)
{
    UnZip unzip(std::move(zipFile));
    return unzip.extractFile(outDir);
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
        return {};

    std::array <char,1024> out {};

    // allocate inflate state
    z_stream strm {};

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
        return {};

    QByteArray result;

    // run deflate()
    strm.avail_out = 0;
    while (strm.avail_out == 0)
    {
        strm.avail_out = out.size();
        strm.next_out  = (Bytef*)(out.data());

        ret = deflate(&strm, Z_FINISH);

        Q_ASSERT(ret != Z_STREAM_ERROR);  // state not clobbered

        switch (ret)
        {
            case Z_NEED_DICT:
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                (void)deflateEnd(&strm);
                return {};
        }

        result.append(out.data(), out.size() - strm.avail_out);
    }

    // clean up and return

    deflateEnd(&strm);

    return result;
}

QByteArray gzipUncompress(const QByteArray &data)
{
    if (data.length() == 0)
        return {};

    std::array<char,1024> out {};

    // allocate inflate state
    z_stream strm;
    strm.total_in = 0;
    strm.total_out = 0;
    strm.zalloc   = Z_NULL;
    strm.zfree    = Z_NULL;
    strm.opaque   = Z_NULL;
    strm.avail_in = data.length();
    strm.next_in  = (Bytef*)(data.data());

    int ret = inflateInit2(&strm, 15 + 16);

    if (ret != Z_OK)
        return {};

    QByteArray result;

    strm.avail_out = 0;
    while (strm.avail_out == 0)
    {
        strm.avail_out = out.size();
        strm.next_out = (Bytef*)out.data();
        ret = inflate(&strm, Z_NO_FLUSH);

        Q_ASSERT(ret != Z_STREAM_ERROR);  // state not clobbered

        switch (ret)
        {
            case Z_NEED_DICT:
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                (void) deflateEnd(&strm);
                return {};
        }

        result.append(out.data(), out.size() - strm.avail_out);
    }

    (void) inflateEnd(& strm);

    return result;
}

