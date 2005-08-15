#ifndef ROMMETADATA_H_
#define ROMMETADATA_H_

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

bool validRom(QString RomName,QString GameType);

void NES_Meta(QString CRC32, QString* GameTitle, QString* Genre, 
                         int* Year, QString* Country);

void SNES_Meta(QString CRC32, QString* GameTitle, QString* Genre, 
                         int* Year, QString* Country);
  
void PCE_Meta(QString CRC32, QString* GameTitle, QString* Genre, 
                         int* Year, QString* Country);
  
void MAME_Meta(QString CRC32, QString* GameTitle, QString* Genre, 
                         int* Year, QString* Country);
  
void GENESIS_Meta(QString CRC32, QString* GameTitle, QString* Genre, 
                         int* Year, QString* Country);

  
void N64_Meta(QString CRC32, QString* GameTitle, QString* Genre,
                         int* Year, QString* Country);

 
void AMIGA_Meta(QString CRC32, QString* GameTitle, QString* Genre,
                         int* Year, QString* Country);

 
void ATARI_Meta(QString CRC32, QString* GameTitle, QString* Genre,
                         int* Year, QString* Country);

uLong crcinfo(QString romname, int offset);

#endif
