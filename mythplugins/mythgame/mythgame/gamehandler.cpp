#include "gamehandler.h"
#include "rominfo.h"
#include "rom_metadata.h"

#include <qobject.h>
#include <qptrlist.h>
#include <qstringlist.h>
#include <iostream>
#include <qdir.h>
#include <qregexp.h>

#include <mythtv/mythcontext.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/mythdialogs.h>
#include <mythtv/util.h>

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

void GameHandler::InitMetaDataMap(QString GameType) 
{
    QString key;

    MSqlQuery query(MSqlQuery::InitCon());
    QString thequery = QString("SELECT crc, category, year, country, name, "
                               "description, publisher, platform, version, "
                               "binfile FROM romdb WHERE platform = \"%1\";")
                              .arg(GameType);
    query.exec(thequery);

    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            key = QString("%1:%2")
                  .arg(query.value(0).toString())
                  .arg(query.value(9).toString());
            romDB[key] = RomData(
                                         query.value(1).toString(),
                                         query.value(2).toString(),
                                         query.value(3).toString(),
                                         query.value(4).toString(),
                                         query.value(5).toString(),
                                         query.value(6).toString(),
                                         query.value(7).toString(),
                                         query.value(8).toString());
        }
    }

    if (romDB.count() == 0)
        cerr << "No romDB data read from database. Not imported?" << endl;
    else
        cerr << "Loaded " << romDB.count() << " items from romDB Database" << endl;

}

void GameHandler::GetMetadata(GameHandler *handler, QString rom, QString* Genre, QString* Year,
                              QString* Country, QString* CRC32, QString* GameName,
                              QString *Publisher, QString *Version)
{
    uLong crc;
    QString key;

    crc = crcinfo(rom, handler->GameType(), &key, &romDB);

    //cerr << "Key = " << key << endl;

    *CRC32 = QString("%1").arg( crc, 0, 16 );
    if (*CRC32 == "0")
        *CRC32 = "";


    // Set our default values
    *Year = QObject::tr("19xx");
    *Country = QObject::tr("Unknown");
    *GameName = QObject::tr("Unknown");
    *Genre = QObject::tr("Unknown");
    *Publisher = QObject::tr("Unknown");
    *Version = QObject::tr("0");

    if (*CRC32 != "")
    {
        if (romDB.contains(key)) 
        {
            *Year = romDB[key].Year();
            *Country = romDB[key].Country();
            *Genre = romDB[key].Genre();
            *Publisher = romDB[key].Publisher();
            *GameName = romDB[key].GameName();
            *Version = romDB[key].Version();
        }
    };

    if (*Genre == "Unknown")
        *Genre = QString("Unknown%1").arg( handler->GameType() );
    
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


static void UpdateGameCounts(QStringList updatelist)
{
    MSqlQuery query(MSqlQuery::InitCon());

    QRegExp multiDiskRGXP = QRegExp( "[0-4]$", TRUE, FALSE );
    int diskcount = 0;
    int pos = 0;

    QString lastrom = "";
    QString firstname = "";
    QString basename = "";

    for ( QStringList::Iterator it = updatelist.begin(); it != updatelist.end(); ++it ) 
    {
        diskcount = 0;
        QString GameType = *it;
        cerr << "Update gametype " << GameType << endl;
        QString romquery = QString("SELECT romname,system,spandisks,gamename FROM gamemetadata,gameplayers WHERE gamemetadata.gametype = \"%1\" AND playername = system ORDER BY romname").arg(GameType);
        query.exec(romquery);
        if (query.isActive() && query.size() > 0)
        {   
            while (query.next())
            {   
                QString RomName = query.value(0).toString();
                QString System = query.value(1).toString();
                int spandisks = query.value(2).toInt();
                QString GameName = query.value(3).toString(); 
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

void GameHandler::UpdateGameDB(GameHandler *handler)
{
    int counter = 0;
    MSqlQuery query(MSqlQuery::InitCon());

    MythProgressDialog progressDlg(QString("Updating %1(%2) Rom database")
                                   .arg(handler->SystemName())
                                   .arg(handler->GameType()),
                                   m_GameMap.size());

    GameScanMap::Iterator iter;

    QString GameName;
    QString Genre;
    QString Country;
    QString CRC32;
    QString thequery;
    QString queryvalues;
    QString Year;
    QString Publisher;
    QString Version;

    int removalprompt = gContext->GetSetting("GameRemovalPrompt").toInt();
    int indepth = gContext->GetSetting("GameDeepScan").toInt();

    for (iter = m_GameMap.begin(); iter != m_GameMap.end(); iter++)
    {

        if (iter.data().FoundLoc() == inFileSystem)
        {
            if (indepth)
            {
                GetMetadata(handler, iter.data().RomFullPath(), &Genre, &Year, &Country, &CRC32, &GameName,
                            &Publisher, &Version);
            }
            else
            {
                Genre = QObject::tr("Unknown");
                Country = QObject::tr("Unknown");
                CRC32 = "";
                Year = QObject::tr("19xx");
                GameName = QObject::tr("Unknown");
                Publisher = QObject::tr("Unknown");
                Version = QObject::tr("0");
            }

            if (GameName == QObject::tr("Unknown")) 
                GameName = iter.data().GameName();

                // Put the game into the database.
                // Had to break the values up into 2 pieces since QString only allows 9 arguments and we had 10

            thequery = QString("INSERT INTO gamemetadata "
                               "(system, romname, gamename, genre, year, gametype, rompath, country, crc_value,"
                               " diskcount, display, publisher, version) ");
            queryvalues = QString ("VALUES (\"%1\", \"%2\", \"%3\", \"%4\", \"%5\", \"%6\",")
                .arg(handler->SystemName())
                .arg(iter.data().Rom().latin1())
                .arg(GameName.latin1())
                .arg(Genre.latin1())
                .arg(Year.latin1())
                .arg(handler->GameType());

            queryvalues.append( QString("\"%1\", \"%2\", \"%3\", 1 ,\"1\", \"%4\", \"%5\");")
                .arg(iter.data().RomPath().latin1())
                .arg(Country.latin1())
                .arg(CRC32)
                .arg(Publisher)
                .arg(Version));

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
    GameScanMap::Iterator iter;

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

				// If we can't read it's contents move on
    if (!RomDir.isReadable()) 
        return 0;

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

                                // If we can't read it's contents move on
    if (!RomDir.isReadable()) 
        return;

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
            cout << "Found Rom : (" << handler->SystemName() << ") " << " - " << RomName << endl;
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
                           "diskcount, display, publisher, version) "
                           "VALUES (\"%1\", \"%2\", \"%3\", \"UnknownPC\", \"19xx\" , \"%4\", "
                           "\"Unknown\",1,1,\"Unknown\", \"0\");")
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
        {
            InitMetaDataMap(handler->GameType());

            UpdateGameDB(handler);

            romDB.clear();
            handler->setRebuild(true);
        } 
        else
            handler->setRebuild(false);

    }

    pdial.Close();
}

void GameHandler::processAllGames(void)
{
    checkHandlers();
    QStringList updatelist;

    GameHandler *handler = handlers->first();

    while(handler)
    {
	updateSettings(handler);
        handler->processGames(handler);

        if (handler->needRebuild())
            updatelist.append(handler->GameType());

        handler = handlers->next();
    }

    if (!updatelist.isEmpty()) 
        UpdateGameCounts(updatelist);
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

