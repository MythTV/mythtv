#include <string>
#include <zlib.h>
#include <qdir.h>
#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>

#include "neshandler.h"
#include "nesrominfo.h"
#include "nessettingsdlg.h"
#include "unzip.h"

#include <iostream>

#include <mythtv/mythdialogs.h>

using namespace std;

NesHandler* NesHandler::pInstance = 0;
const char* NesHandler::Magic = "NES\032";

NesHandler* NesHandler::getHandler()
{
    if(!pInstance)
    {
        pInstance = new NesHandler();
    }
    return pInstance;
}

void NesHandler::start_game(RomInfo* romdata)
{
    QString exec = gContext->GetSetting("NesBinary") + " " +
                   "\"" + gContext->GetSetting("NesRomLocation") + "/" + 
                   romdata->Romname() + "\"";
    cout << exec << endl;
    
    // Run the emulator and wait for it to terminate.      
    FILE* command = popen(exec, "w");
    pclose(command);
}

void NesHandler::edit_settings(RomInfo* romdata)
{
    romdata = romdata;

    NesSettingsDlg settingsdlg(romdata->Romname().latin1());
    settingsdlg.exec();
}

void NesHandler::edit_system_settings(RomInfo* romdata)
{
    romdata = romdata;

    NesSettingsDlg settingsdlg("default");
    settingsdlg.exec();
}

void NesHandler::processGames()
{

    // Remove all metadata entries from the tables, all correct values will be
    // added as they are found.  This is done so that entries that may no longer be
    // available or valid are removed each time the game list is remade.
    MSqlQuery query(MSqlQuery::InitCon());
    query.exec("DELETE FROM gamemetadata WHERE system = \"Nes\";");

    // Search the rom dir for valid new roms.
    QDir RomDir(gContext->GetSetting("NesRomLocation"));
    const QFileInfoList* List = RomDir.entryInfoList();

    if (!List)
        return;

    MythProgressDialog pdial(QObject::tr("Looking for NES games..."), List->count());
    int progress = 0;

    for (QFileInfoListIterator it(*List); it; ++it)
    {
        pdial.setProgress(progress);
        progress++;

        QFileInfo Info(*it.current());
        if (IsNesRom(Info.filePath()))
        {
            QString GameName = GetGameName(Info.filePath());
            if (GameName.isNull())
            {
                GameName = Info.fileName();
            }
            cout << GameName << endl;

            QString Genre(QObject::tr("Unknown"));
            int Year = 0;
            GetMetadata(GameName, &Genre, &Year);
            
            // Put the game into the database.
            QString thequery = QString("INSERT INTO gamemetadata "
                               "(system, romname, gamename, genre, year) "
                               "VALUES (\"Nes\", \"%1\", \"%2\", \"%3\", %4);")
                               .arg(Info.fileName().latin1())
                               .arg(GameName.latin1()).arg(Genre.latin1())
                               .arg(Year);
            query.exec(thequery);
        }
        else
        {
            // Unknown type of file.
        }
    }
   
    pdial.Close();
}

RomInfo* NesHandler::create_rominfo(RomInfo* parent)
{
    return new NesRomInfo(*parent);
}

bool NesHandler::IsNesRom(QString Path)
{
    bool NesRom = false;
    char First4Bytes[4];

    // See if the file is a zip file first
    unzFile zf;
    if ((zf = unzOpen(Path)))
    {
        // TODO Does this loop check the last file?
        int FoundFile;
        for (FoundFile = unzGoToFirstFile(zf); FoundFile == UNZ_OK;
             FoundFile = unzGoToNextFile(zf))
        {
            if (unzOpenCurrentFile(zf) == UNZ_OK)
            {
                unzReadCurrentFile(zf, First4Bytes, 4);
                if (strncmp(Magic, First4Bytes, 4) == 0)
                {
                    // We found a NES rom in the zip, so exit the loop.
                    // Assume that this is the one the emulator will find
                    // also.
                    NesRom = true;
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
        // Normal file
        QFile qf(Path);
        if (qf.open(IO_ReadOnly))
        {
            // Use the magic number for iNes files to check against the file.
            qf.readBlock(First4Bytes, 4);
            if (strncmp(Magic, First4Bytes, 4) == 0)
            {
                NesRom = true;
            }
            qf.close();
        }
    }

    return  NesRom;
}

QString NesHandler::GetGameName(QString Path)
{
    static bool bCRCMapLoaded = false;
    static map<QString, QString> CRCMap;

    // Load the CRC -> GoodName map if we haven't already.
    if (!bCRCMapLoaded) 
    {
        LoadCRCFile(CRCMap);
        bCRCMapLoaded = true;
    }

    // Try to get the GoodNES name for this file.
    QString GoodName;

    // Get CRC of file
    char block[8192];
    uLong crc = crc32(0, Z_NULL, 0);

    unzFile zf;
    if ((zf = unzOpen(Path)))
    {
        // Find NES Rom in zip
        char First4Bytes[4];
        // TODO Does this loop check the last file?
        int FoundFile;
        for (FoundFile = unzGoToFirstFile(zf); FoundFile == UNZ_OK;
             FoundFile = unzGoToNextFile(zf))
        {
            if (unzOpenCurrentFile(zf) == UNZ_OK)
            {
                unzReadCurrentFile(zf, First4Bytes, 4);
                if (strncmp(Magic, First4Bytes, 4) == 0)
                {
                    // We found a NES rom in the zip, so exit the loop.
                    // Assume that this is the one the emulator will find
                    // also.

                    // Skip past iNes header
                    unzReadCurrentFile(zf, block, 16);

                    // Get CRC of rom data
                    int count;
                    while ((count = unzReadCurrentFile(zf, block, 8192)))
                    {
                        crc = crc32(crc, (Bytef *)block, (uInt)count);
                    }

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
        QFile f(Path);
        if (f.open(IO_ReadOnly))
        {
            // Skip past iNes header
            f.readBlock(block, 16);

            // Get CRC of rom data
            Q_LONG count;
            while ((count = f.readBlock(block, 8192)))
            {
                crc = crc32(crc, (Bytef *)block, (uInt)count);
            }

            f.close();
        }
    }

    QString CRC;
    CRC.setNum(crc, 16);

    // Match CRC against crc file
    map<QString, QString>::iterator i;
    if ((i = CRCMap.find(CRC)) != CRCMap.end())
    {
        GoodName = i->second;
    }

    return GoodName;
}

void NesHandler::LoadCRCFile(map<QString, QString> &CRCMap)
{
    QString CRCFilePath = gContext->GetSetting("NesCRCFile");
    QFile CRCFile(CRCFilePath);
    if (CRCFile.open(IO_ReadOnly)) 
    {
        QString line;
        while (CRCFile.readLine(line, 256) != -1) 
        {
            if (line[0] == '#') 
            {
                continue;
            }

            QStringList fields(QStringList::split("|", line));
            QStringList CRCName(QStringList::split("=", fields.first()));
            QString CRC(CRCName.first());
            CRCName.pop_front();
            QString Name(CRCName.first());
      
            if (!CRC.isNull() && !Name.isNull())
            {
                CRCMap[CRC] = Name.stripWhiteSpace();
            }
        }
        CRCFile.close();
    }
}

void NesHandler::GetMetadata(QString GameName, QString* Genre, int* Year)
{
    // Try to match the GoodNES name against the title table to get the metadata.
    
    QString thequery;
    thequery = QString("SELECT releasedate, keywords FROM nestitle "
                       "WHERE MATCH(description) AGAINST ('%1');")
                       .arg(GameName);

    MSqlQuery query(MSqlQuery::InitCon());
    query.exec(thequery);

    if (query.isActive() && query.size() > 0)
    {
        // Take first entry since that will be the most relevant match.
        query.first();
        *Year = query.value(0).toInt();

        // To get the genre, use the first keyword that doesn't count the number
        // of players.
        QStringList keywords(QStringList::split(" ", query.value(1).toString()));
        for (QStringList::Iterator keyword = keywords.begin(); keyword != keywords.end(); ++keyword)
        {
            if ((*keyword)[0].isDigit())
                continue;

            thequery = QString("SELECT value FROM neskeyword WHERE keyword = '%1';").arg(*keyword);
            query.exec(thequery);

            if (query.isActive() && query.size() > 0)
            {
                query.first();
                *Genre = query.value(0).toString();
                break;
            }
        }
    }
}
