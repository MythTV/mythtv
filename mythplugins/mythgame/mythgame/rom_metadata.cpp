// C++
#include <array>

// Qt
#include <QFile>

// MythTV
#include <libmyth/mythcontext.h>

// MythGame
#include "rom_metadata.h"

#include "zlib.h"
#undef Z_NULL
#define Z_NULL nullptr
#include "zip.h"

static int calcOffset(const QString& GameType, uint32_t filesize) {
    int result = 0;

    if (GameType == "NES")
    {
        result = 16;
    }
    else if (GameType == "SNES")
    {
         uint32_t rom_size = (filesize / 0x2000) * 0x2000;

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

static QString crcStr(uint32_t crc)
{
    QString tmpcrc("");

    tmpcrc = QString("%1").arg( crc, 0, 16 );
    if (tmpcrc == "0")
        tmpcrc = "";
    else
        tmpcrc = tmpcrc.rightJustified(8, '0');

    return tmpcrc;
}

// NOLINTNEXTLINE(readability-uppercase-literal-suffix)
static constexpr uint64_t STATS_REQUIRED {ZIP_STAT_NAME|ZIP_STAT_INDEX|ZIP_STAT_SIZE};

// Return the crc32 info for this rom. (ripped mostly from the old neshandler.cpp source)
QString crcinfo(const QString& romname, const QString& GameType, QString *key, RomDBMap *romDB)
{
    // Get CRC of file
    std::array<char,32768> block {};
    uint32_t crc = crc32(0, nullptr, 0);
    QString crcRes;

    int blocksize = 8192;
#if 0
    LOG(VB_GENERAL, LOG_DEBUG,
        QString("crcinfo : %1 : %2 :").arg(romname).arg(GameType));
#endif

    int err { ZIP_ER_OK };
    zip_t *zf = zip_open(qPrintable(romname), 0, &err);
    if (zf != nullptr)
    {
        zip_int64_t numEntries = zip_get_num_entries(zf, 0);
        for (int index = 0; index < numEntries; index++)
        {
            zip_file_t *infile = zip_fopen_index(zf, index, 0);
            if (infile)
            {
                zip_stat_t stats;
                zip_stat_init(&stats);
                zip_stat_index(zf, index, 0, &stats);
                if ((stats.valid & STATS_REQUIRED) != STATS_REQUIRED)
                    continue;

                int offset = calcOffset(GameType, stats.size);

                if (offset > 0)
                    zip_fread(infile, block.data(), offset);

                // Get CRC of rom data
                int count = 0;
                while ((count = zip_fread(infile, block.data(), blocksize)) > 0)
                {
                    crc = crc32(crc, (Bytef *)block.data(), (uInt)count);
                }
                crcRes = crcStr(crc);
                *key = QString("%1:%2").arg(crcRes, stats.name);

                if (romDB->contains(*key))
                {
                    zip_fclose(infile);
                    break;
                }
                zip_fclose(infile);
            }
        }
        zip_close(zf);
    }
    else
    {
        QFile f(romname);

        if (f.open(QIODevice::ReadOnly))
        {
            int offset = calcOffset(GameType, f.size());

            if (offset > 0)
                f.read(block.data(), offset);

            // Get CRC of rom data
            qint64 count = 0;
            while ((count = f.read(block.data(), blocksize)) > 0)
            {
                crc = crc32(crc, (Bytef *)block.data(), (uInt)count);
            }

            crcRes = crcStr(crc);
            *key = QString("%1:").arg(crcRes);
            f.close();
        }
    }

    return crcRes;
}

