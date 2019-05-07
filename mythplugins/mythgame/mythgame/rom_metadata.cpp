#include "config.h"
#include "rom_metadata.h"

#include <QFile>

#include <mythcontext.h>

#include "zlib.h"
#include MINIZIP_UNZIP_H
#undef Z_NULL
#define Z_NULL nullptr

static int calcOffset(const QString& GameType, uLong filesize) {
    int result;
    uLong rom_size;

    result = 0;

    if (GameType == "NES")
    {
        result = 16;
    }
    else if (GameType == "SNES")
    {
         rom_size = (filesize / 0x2000) * 0x2000;

         if (rom_size < filesize)
             result = filesize - rom_size;
    }
    else if (GameType == "PCE")
    {
         if (filesize & 0x0FFF)
             result = filesize & 0x0FFF;

    }

    return result;
}

static QString crcStr(uLong crc)
{
    QString tmpcrc("");

    tmpcrc = QString("%1").arg( crc, 0, 16 );
    if (tmpcrc == "0")
        tmpcrc = "";
    else
        tmpcrc = tmpcrc.rightJustified(8, '0');

    return tmpcrc;
}

// Return the crc32 info for this rom. (ripped mostly from the old neshandler.cpp source)
QString crcinfo(const QString& romname, const QString& GameType, QString *key, RomDBMap *romDB)
{
    // Get CRC of file
    char block[32768] = "";
    uLong crc = crc32(0, Z_NULL, 0);
    QString crcRes;
    unz_file_info file_info;
    int offset;
    unzFile zf;
    int blocksize;

    blocksize = 8192;
#if 0
    LOG(VB_GENERAL, LOG_DEBUG,
        QString("crcinfo : %1 : %2 :").arg(romname).arg(GameType));
#endif

    if ((zf = unzOpen(qPrintable(romname))))
    {
        int FoundFile;
        for (FoundFile = unzGoToFirstFile(zf); FoundFile == UNZ_OK;
             FoundFile = unzGoToNextFile(zf))
        {
            if (unzOpenCurrentFile(zf) == UNZ_OK)
            {
                char filename_inzip[256];
                unzGetCurrentFileInfo(zf,&file_info,filename_inzip,sizeof(filename_inzip),nullptr,0,nullptr,0);

                offset = calcOffset(GameType, file_info.uncompressed_size);

                if (offset > 0)
                    unzReadCurrentFile(zf, block, offset);

                // Get CRC of rom data
                int count;
                while ((count = unzReadCurrentFile(zf, block, blocksize)) > 0)
                {
                    crc = crc32(crc, (Bytef *)block, (uInt)count);
                }
                crcRes = crcStr(crc);
                *key = QString("%1:%2")
                             .arg(crcRes)
                             .arg(filename_inzip);

                if (romDB->contains(*key))
                {
                    unzCloseCurrentFile(zf);
                    break;
                }

                unzCloseCurrentFile(zf);
            }
        }
        unzClose(zf);
    }
    else
    {
        QFile f(romname);

        if (f.open(QIODevice::ReadOnly))
        {
            offset = calcOffset(GameType, f.size());

            if (offset > 0)
                f.read(block, offset);

            // Get CRC of rom data
            qint64 count;
            while ((count = f.read(block, blocksize)) > 0)
            {
                crc = crc32(crc, (Bytef *)block, (uInt)count);
            }

            crcRes = crcStr(crc);
            *key = QString("%1:").arg(crcRes);
            f.close();
        }
    }

    return crcRes;
}

