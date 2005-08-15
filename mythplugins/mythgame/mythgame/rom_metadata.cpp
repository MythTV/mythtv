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

bool validRom(QString RomName,QString GameType)
{
    // Add proper checks here
    return TRUE;  
}

void NES_Meta(QString CRC32, QString* GameTitle, QString* Genre, 
                         int* Year, QString* Country)
{

    *Year = 1970;
    *Country = QObject::tr("Unknown");
    *Genre = QObject::tr("UnknownNES");

}
  
void SNES_Meta(QString CRC32, QString* GameTitle, QString* Genre, 
                         int* Year, QString* Country)
{   

    *Year = 1970;
    *Country = QObject::tr("Unknown");
    *Genre = QObject::tr("UnknownSNES");

}

void PCE_Meta(QString CRC32, QString* GameTitle, QString* Genre,
                         int* Year, QString* Country)
{   

    *Year = 1970;
    *Country = QObject::tr("Unknown");
    *Genre = QObject::tr("UnknownPCE");

}

void MAME_Meta(QString CRC32, QString* GameTitle, QString* Genre,
                         int* Year, QString* Country)
{   

    *Year = 1970;
    *Country = QObject::tr("Unknown");
    *Genre = QObject::tr("UnknownMAME");

}


void GENESIS_Meta(QString CRC32, QString* GameTitle, QString* Genre,
                         int* Year, QString* Country)
{   

    *Year = 1970;
    *Country = QObject::tr("Unknown");
    *Genre = QObject::tr("UnknownGENESIS");

}

 
void N64_Meta(QString CRC32, QString* GameTitle, QString* Genre,
                         int* Year, QString* Country)
{   

    *Year = 1970;
    *Country = QObject::tr("Unknown");
    *Genre = QObject::tr("UnknownN64");

}


void AMIGA_Meta(QString CRC32, QString* GameTitle, QString* Genre,
                         int* Year, QString* Country)
{   

    *Year = 1970;
    *Country = QObject::tr("Unknown");
    *Genre = QObject::tr("UnknownAMIGA");

}


void ATARI_Meta(QString CRC32, QString* GameTitle, QString* Genre,
                         int* Year, QString* Country)

{   

    *Year = 1970;
    *Country = QObject::tr("Unknown");
    *Genre = QObject::tr("UnknownATARI");

}

// Return the crc32 info for this rom. (ripped mostly from the old neshandler.cpp source)
uLong crcinfo(QString romname, int offset)
{
    // Get CRC of file
    char block[32768];
    uLong crc = crc32(0, Z_NULL, 0);
    
    unzFile zf;
    if ((zf = unzOpen(romname)))
    {
        int FoundFile;
        for (FoundFile = unzGoToFirstFile(zf); FoundFile == UNZ_OK;
             FoundFile = unzGoToNextFile(zf))
        {
            if (unzOpenCurrentFile(zf) == UNZ_OK)
            {

                // Skip past iNes header
                if (offset > 0)
                    unzReadCurrentFile(zf, block, offset);

                // Get CRC of rom data
                int count;
                while ((count = unzReadCurrentFile(zf, block, 32768)))
                {
                    crc = crc32(crc, (Bytef *)block, (uInt)count);
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
            if (offset > 0)
                f.readBlock(block, offset);

            // Get CRC of rom data
            Q_LONG count;
            while ((count = f.readBlock(block, 32768)))
            {
                crc = crc32(crc, (Bytef *)block, (uInt)count);
            }   

            f.close();
        }   
    }   
    
    return crc;
}

