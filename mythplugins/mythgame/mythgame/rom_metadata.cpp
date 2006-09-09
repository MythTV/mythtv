#include <qobject.h>
#include <qptrlist.h>
#include <qstringlist.h>
#include <iostream>
#include <qdir.h>
#include <qstring.h>
#include <qwidget.h>


#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/mythdialogs.h>

#include "rom_metadata.h"
#include "unzip.h"

int calcOffset(QString GameType, uLong filesize) {
    int result;
    uLong rom_size;

    result = 0;

    if (GameType == "NES") {
        result = 16;
    }
    else if (GameType == "SNES") {
         rom_size = (filesize / 0x2000) * 0x2000;

         if (rom_size < filesize)
             result = filesize - rom_size;
    }
    else if (GameType == "PCE") {
         if (filesize & 0x0FFF)
             result = filesize & 0x0FFF;

    }

    return result;
}

// Return the crc32 info for this rom. (ripped mostly from the old neshandler.cpp source)
uLong crcinfo(QString romname, QString GameType, QString *key, RomDBMap *romDB)
{
    // Get CRC of file
    char block[32768];
    uLong crc = crc32(0, Z_NULL, 0);

    char filename_inzip[256];
    unz_file_info file_info;    
    int err;

    int offset;
    unzFile zf;
    int blocksize;

    blocksize = 8192;

    if ((zf = unzOpen(romname)))
    {
        int FoundFile;
        for (FoundFile = unzGoToFirstFile(zf); FoundFile == UNZ_OK;
             FoundFile = unzGoToNextFile(zf))
        {
            if (unzOpenCurrentFile(zf) == UNZ_OK)
            {
                err = unzGetCurrentFileInfo(zf,&file_info,filename_inzip,sizeof(filename_inzip),NULL,0,NULL,0);

                offset = calcOffset(GameType, file_info.uncompressed_size);

                if (offset > 0)
                    unzReadCurrentFile(zf, block, offset);

                // Get CRC of rom data
                int count;
                while ((count = unzReadCurrentFile(zf, block, blocksize)) > 0)
                {
                    crc = crc32(crc, (Bytef *)block, (uInt)count);
                }   

                *key = QString("%1:%2")
                             .arg(crc, 0, 16)
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
        if (f.open(IO_ReadOnly))
        {
            offset = calcOffset(GameType, f.size());

            if (offset > 0)
                f.readBlock(block, offset);

            // Get CRC of rom data
            Q_LONG count;
            while ((count = f.readBlock(block, blocksize)) > 0)
            {
                crc = crc32(crc, (Bytef *)block, (uInt)count);
            }   
            *key = QString("%1:").arg(crc, 0, 16);
            f.close();
        }   
    }   

    return crc;
}

