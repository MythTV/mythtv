#include "gamehandler.h"
#include "rominfo.h"
#include "rom_metadata.h"

#include <qobject.h>
#include <qptrlist.h>
#include <qstringlist.h>
#include <iostream>
#include <qdir.h>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/mythdialogs.h>


using namespace std;

static QPtrList<GameHandler> *handlers = 0;

bool existsHandler(const QString name)
{
    GameHandler *handler = handlers->first();
    while(handler)
    {
        if (handler->SystemName() == name)
        {
            return TRUE;
        }
        
        handler = handlers->next();
    }

    return FALSE;
}

static void checkHandlers(void)
{
    // If a handlers list doesn't currently exist create one. Otherwise
    // clear the existing list so that we can regenerate a new one.
    if (! handlers)
    {   
        handlers = new QPtrList<GameHandler>;
    }
    else {
        handlers->clear();
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.exec("SELECT DISTINCT playername FROM gameplayers WHERE playername <> '';");
    while (query.next()) 
    {   
        QString name = query.value(0).toString();
        GameHandler::registerHandler(GameHandler::newHandler(name));
    }   

}

GameHandler* GameHandler::getHandler(uint i)
{
    return handlers->at(i);
}

void GameHandler::updateSettings(GameHandler *handler)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.exec("SELECT rompath, workingpath, commandline, screenshots, gameplayerid, gametype, extensions, spandisks  FROM gameplayers WHERE playername = \"" + handler->SystemName() + "\";");

    query.next();
    handler->rompath = query.value(0).toString();
    handler->workingpath = query.value(1).toString();
    handler->commandline = query.value(2).toString();
    handler->screenshots = query.value(3).toString();
    handler->gameplayerid = query.value(4).toInt();
    handler->gametype = query.value(5).toString();
    handler->validextensions = QStringList::split(",", query.value(6).toString().stripWhiteSpace().remove(" "));
    handler->spandisks = query.value(7).toInt();

}

GameHandler* GameHandler::newInstance = 0;

GameHandler* GameHandler::newHandler(QString name)
{
    newInstance = new GameHandler();
    newInstance->systemname = name;

    updateSettings(newInstance);

    return newInstance;
}

// Creates/rebuilds the handler list and then returns the count.
uint GameHandler::count(void)
{
    checkHandlers();
    return handlers->count();
}


void GameHandler::GetMetadata(GameHandler *handler, QString rom, QString* Genre, int* Year,
                              QString* Country, QString* CRC32)
{
    uLong crc;

    if (handler->GameType() == "NES")
        crc = crcinfo(rom, 16);
    else
        crc = crcinfo(rom,0);

    *CRC32 = QString("%1").arg( crc, 0, 16 );
    if (*CRC32 == "0")
        *CRC32 = "";


    // Set our default values
    *Year = 0;
    *Country = QObject::tr("Unknown");
    *Genre = QObject::tr("Unknown");


    // Currently this is discard, but we'll use this to fill in GameName
    QString GameTitle;
    // Now if possible fill them in with more detailed information.
    if (handler->GameType() == "NES")
        NES_Meta(*CRC32, &GameTitle, Genre, Year, Country); 

    else if (handler->GameType() == "SNES")
        SNES_Meta(*CRC32, &GameTitle, Genre, Year, Country);

    else if (handler->GameType() == "PCE")
        PCE_Meta(*CRC32, &GameTitle, Genre, Year, Country);

    else if (handler->GameType() == "MAME")
        MAME_Meta(*CRC32, &GameTitle, Genre, Year, Country);

    else if (handler->GameType() == "GENESIS")
        GENESIS_Meta(*CRC32, &GameTitle, Genre, Year, Country);

    else if (handler->GameType() == "N64")
        N64_Meta(*CRC32, &GameTitle, Genre, Year, Country);

    else if (handler->GameType() == "AMIGA")
        AMIGA_Meta(*CRC32, &GameTitle, Genre, Year, Country);

    else if (handler->GameType() == "ATARI")
        ATARI_Meta(*CRC32, &GameTitle, Genre, Year, Country);

    else 
        cout << "No Meta routines for this game type : " << handler->GameType() << endl;

}

void purgeGameDB(QString filename, QString RomPath) 
{
    cerr << "Purging " << RomPath << " - " << filename << endl;
    MSqlQuery query(MSqlQuery::InitCon());

    // This should have the added benefit of removing the rom from
    // other games of the same gametype so we wont be asked to remove it
    // more than once.
    QString thequery = QString("DELETE FROM gamemetadata WHERE romname = \"%1\" AND "
                                " rompath = \"%2\"; ")
                              .arg(filename)
                              .arg(RomPath);
    query.exec(thequery);


}

void GameHandler::promptForRemoval(QString filename, QString RomPath)
{
    if (m_RemoveAll)
        purgeGameDB(filename , RomPath);

    if (m_KeepAll || m_RemoveAll)
        return;

    QStringList buttonText;
    buttonText += QObject::tr("No");
    buttonText += QObject::tr("No to all");
    buttonText += QObject::tr("Yes");
    buttonText += QObject::tr("Yes to all");


    int result = MythPopupBox::showButtonPopup(gContext->GetMainWindow(), 
                               QObject::tr("File Missing"),
                               QString(QObject::tr("%1 appears to be missing.\nRemove it"
                                                   " from the database?")).arg(filename),
                                                    buttonText, 0 );
    switch (result)
    {
        case 1:
            m_KeepAll = true;
            break;
        case 2:
            purgeGameDB(filename , RomPath);
            break;
        case 3:
            m_RemoveAll = true;
            purgeGameDB(filename , RomPath);
            break;
    };

}

void updateDisplayRom(QString romname, int display, QString Systemname)
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString thequery = QString("UPDATE gamemetadata SET display = %1 WHERE romname = \"%2\" AND "
                                " system = \"%3\"; ")
                              .arg(display)
                              .arg(romname)
                              .arg(Systemname);
    query.exec(thequery);

}

void updateDiskCount(QString romname, int diskcount, QString GameType)
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString thequery = QString("UPDATE gamemetadata SET diskcount = %1 WHERE romname = \"%2\" AND "
                                " gametype = \"%3\"; ")
                              .arg(diskcount)
                              .arg(romname)
                              .arg(GameType);
    query.exec(thequery);

}

void updateGameName(QString romname, QString GameName, QString Systemname)
{
    MSqlQuery query(MSqlQuery::InitCon());
    QString thequery = QString("UPDATE gamemetadata SET GameName = \"%1\" WHERE romname = \"%2\" AND "
                                " system = \"%3\"; ")
                              .arg(GameName)
                              .arg(romname) 
                              .arg(Systemname);
    query.exec(thequery);

}


static void UpdateGameCounts(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    MSqlQuery query2(MSqlQuery::InitCon());

    QRegExp multiDiskRGXP = QRegExp( "[0-4]$", TRUE, FALSE );
    int diskcount = 0;
    int pos = 0;

    QString lastrom = "";
    QString firstname = "";
    QString basename = "";

    QString gtquery = QString("SELECT DISTINCT gametype FROM gamemetadata");
    query.exec(gtquery);

    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {   
            diskcount = 0;
            QString GameType = query.value(0).toString();
            cerr << "Update gametype " << GameType << endl;
            QString romquery = QString("SELECT romname,system,spandisks,gamename FROM gamemetadata,gameplayers WHERE gamemetadata.gametype = \"%1\" AND playername = system ORDER BY romname").arg(GameType);
            query2.exec(romquery);
            if (query2.isActive() && query2.size() > 0)
            {   
                while (query2.next())
                {   
                    QString RomName = query2.value(0).toString();
                    QString System = query2.value(1).toString();
                    int spandisks = query2.value(2).toInt();
                    QString GameName = query2.value(3).toString(); 
                    int extlength = 0;

                    basename = RomName;

                    if (spandisks)
                    {
                        {

                            pos = RomName.findRev(".");
                            if (pos > 1)
                            {   
                                extlength = RomName.length() - pos;
                                pos--;

                                basename = RomName.mid(pos,1);
                            }

                            if (basename.contains(multiDiskRGXP))
                            {
                                pos = (RomName.length() - extlength) - 1;
                                basename = RomName.left(pos);

                                if (basename.right(1) == ".")
                                    basename = RomName.left(pos - 1);
                            }
                            else
                                basename = GameName;

                            if (basename == lastrom)
                            {
                                updateDisplayRom(RomName,0,System);
                                diskcount++;
                                if (diskcount > 1)
                                    updateDiskCount(firstname,diskcount,GameType);
                            }
                            else
                            {
                                firstname = RomName;
                                lastrom = basename;
                                diskcount = 1;
                            }
                            if (basename != GameName)
                                updateGameName(RomName,basename,System);
                        }
                    } 
                    else
                    {
                        if (basename == lastrom)
                                updateDisplayRom(RomName,0,System);
                            else
                                lastrom = basename;

                    }
                }
            }
        }
    }
}

void GameHandler::UpdateGameDB(GameHandler *handler)
{
    int counter = 0;
    MSqlQuery query(MSqlQuery::InitCon());
    MythProgressDialog progressDlg(QObject::tr("Updating Rom database"),
                                   m_GameMap.size());
    GameDataMap::Iterator iter;

    QString GameName;
    QString Genre;
    QString Country;
    QString CRC32;
    QString thequery;
    QString queryvalues;
    int Year;

    int removalprompt = gContext->GetSetting("GameRemovalPrompt").toInt();
    int indepth = gContext->GetSetting("GameDeepScan").toInt();

    for (iter = m_GameMap.begin(); iter != m_GameMap.end(); iter++)
    {
        GameName = iter.data().GameName();

        if (iter.data().FoundLoc() == inFileSystem)
        {
            Genre = QObject::tr("Unknown");
            Country = QObject::tr("Unknown");
            CRC32 = "";
            Year = 0;

            if (indepth)
                GetMetadata(handler, iter.data().RomFullPath(), &Genre, &Year, &Country, &CRC32);

                // Put the game into the database.
                // Had to break the values up into 2 pieces since QString only allows 9 arguments and we had 10
            thequery = QString("INSERT INTO gamemetadata "
                    "(system, romname, gamename, genre, year, gametype, rompath,country,crc_value,diskcount,display) ");
            queryvalues = QString ("VALUES (\"%1\", \"%2\", \"%3\", \"%4\", %5, \"%6\",")
                .arg(handler->SystemName())
                .arg(iter.data().Rom().latin1())
                .arg(GameName.latin1())
                .arg(Genre.latin1())
                .arg(Year)
                .arg(handler->GameType());

            queryvalues.append( QString("\"%1\", \"%2\", \"%3\", 1 ,\"1\");")
                .arg(iter.data().RomPath().latin1())
                .arg(Country.latin1())
                .arg(CRC32));

            thequery.append(queryvalues);
            query.exec(thequery);

        } 
        else if ((iter.data().FoundLoc() == inDatabase) && (removalprompt))
        {

            promptForRemoval( iter.data().Rom() , iter.data().RomPath() );
        }

        progressDlg.setProgress(++counter);
    }

    progressDlg.Close();
}

void GameHandler::VerifyGameDB(GameHandler *handler)
{
    int counter = 0;
    GameDataMap::Iterator iter;

    MSqlQuery query(MSqlQuery::InitCon());
    query.exec("SELECT romname,rompath,gamename FROM gamemetadata WHERE system = \"" + handler->SystemName() + "\";");

    MythProgressDialog progressDlg(QObject::tr("Verifying " + handler->SystemName() + " files"),
                                   query.numRowsAffected());

    // For every file we know about, check to see if it still exists.
    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            QString RomName = QString::fromUtf8(query.value(0).toString());
            QString RomPath = query.value(1).toString();
            QString GameName = query.value(2).toString();
            if (RomName != QString::null)
            {
                if ((iter = m_GameMap.find(RomName)) != m_GameMap.end())
                {
                    // If it's both on disk and in the database we're done with it.
                    //iter.data().setLoc(inBoth);
                    m_GameMap.remove(iter);
                }
                else
                {
                    // If it's only in the database add it to our list and mark it for
                    // removal.
                    m_GameMap[RomName] = GameScan(RomName,RomPath + "/" + RomName,inDatabase,
                                         GameName,RomPath);
                }
            }
            progressDlg.setProgress(++counter);
        }
    }
    progressDlg.Close();
}

// Recurse through the directory and gather a count on how many files there are to process.
// This is used for the progressbar info.
int GameHandler::buildFileCount(QString directory, GameHandler *handler)
{
    int filecount = 0;
    QDir RomDir(directory);
    const QFileInfoList* List = RomDir.entryInfoList();
    for (QFileInfoListIterator it(*List); it; ++it)
    {   
        QFileInfo Info(*it.current());
        QString RomName = Info.fileName();

        if (RomName == "." ||
            RomName == "..")
        {   
            continue;
        }

        if (Info.isDir())
        {   
            filecount += buildFileCount(Info.filePath(), handler);
            continue;
        }
        else
        {   
            if (handler->validextensions.count() > 0)
            {   
                QRegExp r;

                r.setPattern("^" + Info.extension( FALSE ) + "$");
                r.setCaseSensitive(false);
                QStringList result = handler->validextensions.grep(r);
                if (result.isEmpty()) {
                    continue;
                }
            }
            filecount++;

        }
    }

    return filecount;
}

void GameHandler::clearAllGameData(void)
{
    QStringList buttonText;
    buttonText += QObject::tr("No");
    buttonText += QObject::tr("Yes");

    int result = MythPopupBox::showButtonPopup(gContext->GetMainWindow(),
                               QObject::tr("Are you sure?"),
                               QString(QObject::tr("This will clear all Game Meta Data\n"
                                                   "from the database. Are you sure you\n"
                                                   "want to do this?" )),
                                                    buttonText, 0 );
    switch (result)
    {
        case 0:
            // Do Nothing
            break;
        case 1:
            MSqlQuery query(MSqlQuery::InitCon());
            QString thequery = "DELETE FROM gamemetadata;";
            query.exec(thequery);
            break;
    };
}

void GameHandler::buildFileList(QString directory, GameHandler *handler, 
                                MythProgressDialog *pdial, int* filecount)
{
    QDir RomDir(directory);
    RomDir.setSorting( QDir:: DirsFirst | QDir::Name );
    const QFileInfoList* List = RomDir.entryInfoList();
    for (QFileInfoListIterator it(*List); it; ++it)
    {
        QFileInfo Info(*it.current());
        QString RomName = Info.fileName();
        QString GameName = Info.baseName(TRUE);

        if (RomName == "." ||
            RomName == "..")
        {
            continue;
        }

        if (Info.isDir())
        {
            buildFileList(Info.filePath(), handler, pdial, filecount);
            continue;
        }
        else
        {

            if (handler->validextensions.count() > 0)
            {
                QRegExp r;

                r.setPattern("^" + Info.extension( FALSE ) + "$");
                r.setCaseSensitive(false);
                QStringList result = handler->validextensions.grep(r);
                if (result.isEmpty()) {
                    continue;
                }
            }

            m_GameMap[RomName] = GameScan(RomName,Info.filePath(),inFileSystem,
                                 GameName,Info.dirPath());
//            cout << "Found Rom : (" << handler->SystemName() << ") " << " - " << RomName << endl;
            *filecount = *filecount + 1;
            pdial->setProgress(*filecount);

        }
    }
}

void GameHandler::processGames(GameHandler *handler)
{
    QString thequery;
    int maxcount = 0;
    MSqlQuery query(MSqlQuery::InitCon());

    if ((handler->SystemRomPath()) && (handler->GameType() != "PC"))
    {
        QDir d(handler->SystemRomPath());
        if (d.exists())
            maxcount = buildFileCount(handler->SystemRomPath(),handler);
        else 
        {
            cout << "Rom Path does not exist : " << handler->SystemRomPath() << endl;
            return;
        }
    }
    else
        maxcount = 100;

    MythProgressDialog pdial(QObject::tr("Scanning for " + handler->SystemName() + " game(s)..."), maxcount);
    pdial.setProgress(0);

    if (handler->GameType() == "PC") 
    {
        thequery = QString("INSERT INTO gamemetadata "
                           "(system, romname, gamename, genre, year, gametype, country, "
                           "diskcount, display) "
                           "VALUES (\"%1\", \"%2\", \"%3\", \"Unknown\", 1970 , \"%4\", "
                           "\"Unknown\",1,1);")
                           .arg(handler->SystemName())
                           .arg(handler->SystemName())
                           .arg(handler->SystemName())
                           .arg(handler->GameType());

        query.exec(thequery);

        pdial.setProgress(maxcount);
        // cout << "PC Game " << handler->SystemName() << endl;
    }
    else
    {   
        int filecount = 0;
        buildFileList(handler->SystemRomPath(),handler,&pdial, &filecount);
        VerifyGameDB(handler);

	// If we still have some games in the list then update the database
        if (!m_GameMap.empty())
            UpdateGameDB(handler);
    }

    pdial.Close();
}

void GameHandler::processAllGames(void)
{
    checkHandlers();

    GameHandler *handler = handlers->first();
    while(handler)
    {
	updateSettings(handler);
        handler->processGames(handler);
        handler = handlers->next();
    }

    // Maybe add some flag or check here to see if we have to bother?
    // Or maybe only update counts for specific gametypes/players?
    // We shouldn't need to update counts and display values for ALL
    // games in the system unless an element of that gametype changed.
    UpdateGameCounts();
}

GameHandler* GameHandler::GetHandler(RomInfo *rominfo)
{
    if (!rominfo)
        return NULL;

    GameHandler *handler = handlers->first();
    while(handler)
    {   
        if(rominfo->System() == handler->SystemName())
        {   
            return handler;
        }
        handler = handlers->next();
    }
    return handler;
}

GameHandler* GameHandler::GetHandlerByName(QString systemname)
{
    if (!systemname)
        return NULL;

    GameHandler *handler = handlers->first();
    while(handler)
    {
        if(handler->SystemName() == systemname)
        {
            return handler;
        }
        handler = handlers->next();
    }
    return handler;
}

void GameHandler::Launchgame(RomInfo *romdata, QString systemname)
{
    GameHandler *handler;
    if (systemname) 
    {
        handler = GetHandlerByName(systemname);
    }
    else if (!(handler = GetHandler(romdata)))
    {
        // Couldn't get handler so abort.
        return;
    }
    QString exec = handler->SystemCmdLine();

    if (handler->GameType() != "PC")
    {
        QString arg = "\"" + romdata->Rompath() +
                      "/" + romdata->Romname() + "\"";

        // If they specified a %s in the commandline place the romname
        // in that location, otherwise tack it on to the end of
        // the command.
        if (exec.contains("%s") || handler->SpanDisks())
        {
            exec = exec.replace(QRegExp("%s"),arg);

            if (handler->SpanDisks())
            {
                QRegExp rxp = QRegExp( "%d[0-4]", TRUE, FALSE );

                if (exec.contains(rxp)) 
                {
                    if (romdata->DiskCount() > 1) 
                    {
			// Chop off the extension, .  and last character of the name which we are assuming is the disk #
                        QString basename = romdata->Romname().left(romdata->Romname().length() - (romdata->getExtension().length() + 2));
                        QString extension = romdata->getExtension();
                        QString rom;
                        QString diskid[] = { "%d0", "%d1", "%d2", "%d3", "%d4", "%d5", "%d6" };

                        for (int disk = 1; disk <= romdata->DiskCount(); disk++)
                        {
                            rom = QString("\"%1/%2%3.%4\"")
                                          .arg(romdata->Rompath())
                                          .arg(basename)
                                          .arg(disk)
                                          .arg(extension);
                            exec = exec.replace(QRegExp(diskid[disk]),rom);
                        }
                    } else
                    {   // If there is only one disk make sure we replace %d1 just like %s
                        exec = exec.replace(QRegExp("%d1"),arg);
                    }
                }
            }
        }
        else
        {
            exec = exec + " \"" + 
                   romdata->Rompath() + "/" +
                   romdata->Romname() + "\"";
        }
    }

    QString savedir = QDir::currentDirPath ();
    QDir d;
    if (handler->SystemWorkingPath()) {
        if (!d.cd(handler->SystemWorkingPath()))
        {
            cout << "Failed to change to specified Working Directory : " << handler->SystemWorkingPath() << endl;
        }
    }
       
    cout << "Launching Game : " << handler->SystemName() << " : " << exec << " : " << endl;

    myth_system(exec);

    (void)d.cd(savedir);      
}

RomInfo* GameHandler::CreateRomInfo(RomInfo* parent)
{
    GameHandler *handler;
    if((handler = GetHandler(parent)))
        return new RomInfo(*parent);
    return NULL;
}

void GameHandler::registerHandler(GameHandler *handler)
{
    handlers->append(handler);
}

