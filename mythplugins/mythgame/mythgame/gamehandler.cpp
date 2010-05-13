#include "gamehandler.h"
#include "rominfo.h"
#include "rom_metadata.h"

#include <QObject>
#include <QRegExp>
#include <QDir>
#include <QList>

#include <mythcontext.h>
#include <mythdbcon.h>
#include <mythdialogs.h>
#include <util.h>
#include <mythdb.h>

#define LOC_ERR QString("MythGame:GAMEHANDLER Error: ")
#define LOC QString("MythGame:GAMEHANDLER: ")

static QList<GameHandler*> *handlers = NULL;

bool existsHandler(const QString name)
{
    for (int i = 0; i < handlers->size(); i++)
    {
        GameHandler *handler = handlers->at(i);
        if (handler->SystemName() == name)
            return true;
    }

    return false;
}

static void checkHandlers(void)
{
    // If a handlers list doesn't currently exist create one. Otherwise
    // clear the existing list so that we can regenerate a new one.
    if (!handlers)
        handlers = new QList<GameHandler*>;
    else
    {
        while (!handlers->isEmpty())
            delete handlers->takeFirst();
        handlers->clear();
    }

    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.exec("SELECT DISTINCT playername FROM gameplayers "
                    "WHERE playername <> '';"))
        MythDB::DBError("checkHandlers - selecting playername", query);

    while (query.next()) 
    {
        QString name = query.value(0).toString();
        GameHandler::registerHandler(GameHandler::newHandler(name));
    }
}

GameHandler *GameHandler::getHandler(uint i)
{
    return handlers->at(i);
}

void GameHandler::updateSettings(GameHandler *handler)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("SELECT rompath, workingpath, commandline, screenshots, "
          "gameplayerid, gametype, extensions, spandisks  "
          "FROM gameplayers WHERE playername = :SYSTEM ");

    query.bindValue(":SYSTEM", handler->SystemName());

    if (query.exec() && query.next())
    {
        handler->rompath = query.value(0).toString();
        handler->workingpath = query.value(1).toString();
        handler->commandline = query.value(2).toString();
        handler->screenshots = query.value(3).toString();
        handler->gameplayerid = query.value(4).toInt();
        handler->gametype = query.value(5).toString();
        handler->validextensions = query.value(6).toString().trimmed()
                                        .remove(" ").split(",");
        handler->spandisks = query.value(7).toInt();
    }
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
    query.prepare("SELECT crc, category, year, country, name, "
                  "description, publisher, platform, version, "
                  "binfile FROM romdb WHERE platform = :GAMETYPE;");
                              
    query.bindValue(":GAMETYPE",GameType);

    if (query.exec())
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
        VERBOSE(VB_GENERAL, LOC_ERR + QString("No romDB data read from "
                "database for gametype %1 . Not imported?").arg(GameType));
    else
        VERBOSE(VB_GENERAL, LOC + QString("Loaded %1 items from romDB Database")
                      .arg(romDB.count()));

}

void GameHandler::GetMetadata(GameHandler *handler, QString rom, QString* Genre, QString* Year,
                              QString* Country, QString* CRC32, QString* GameName,
                              QString *Plot, QString *Publisher, QString *Version, 
                              QString* Fanart, QString* Boxart)
{
    QString key;
    QString tmpcrc;

    *CRC32 = crcinfo(rom, handler->GameType(), &key, &romDB);

    //cerr << "Key = " << key << endl;

    // Set our default values
    *Year = QObject::tr("19xx");
    *Country = QObject::tr("Unknown");
    *GameName = QObject::tr("Unknown");
    *Genre = QObject::tr("Unknown");
    *Plot = QObject::tr("Unknown");
    *Publisher = QObject::tr("Unknown");
    *Version = QObject::tr("0");
    (*Fanart).clear();
    (*Boxart).clear();

    if (!(*CRC32).isEmpty())
    {
        if (romDB.contains(key)) 
        {
            VERBOSE(VB_GENERAL, LOC + QString("ROMDB FOUND for %1 - %2")
                     .arg(romDB[key].GameName()).arg(key));
            *Year = romDB[key].Year();
            *Country = romDB[key].Country();
            *Genre = romDB[key].Genre();
            *Publisher = romDB[key].Publisher();
            *GameName = romDB[key].GameName();
            *Version = romDB[key].Version();
        }
        else
        {
            VERBOSE(VB_GENERAL, LOC + QString("NO ROMDB FOUND for %1 (%2)")
                    .arg(rom).arg(*CRC32));
        }

    };

    if ((*Genre == "Unknown") || (*Genre).isEmpty())
        *Genre = QString("Unknown%1").arg( handler->GameType() );
    
}

void purgeGameDB(QString filename, QString RomPath) 
{
    VERBOSE(VB_GENERAL, LOC + QString("Purging %1 - %2").arg(RomPath)
            .arg(filename));

    MSqlQuery query(MSqlQuery::InitCon());

    // This should have the added benefit of removing the rom from
    // other games of the same gametype so we wont be asked to remove it
    // more than once.
    query.prepare("DELETE FROM gamemetadata WHERE "
          "romname = :ROMNAME AND "
                  "rompath = :ROMPATH ");

    query.bindValue(":ROMNAME",filename);
    query.bindValue(":ROMPATH",RomPath);

    if (!query.exec())
        MythDB::DBError("purgeGameDB", query);

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


    DialogCode result = MythPopupBox::ShowButtonPopup(
        GetMythMainWindow(), 
        QObject::tr("File Missing"),
        QString(QObject::tr("%1 appears to be missing.\nRemove it"
                            " from the database?")).arg(filename),
        buttonText, kDialogCodeButton0);

    switch (result)
    {
        case kDialogCodeButton0:
        case kDialogCodeRejected:
        default:
            break;
        case kDialogCodeButton1:
            m_KeepAll = true;
            break;
        case kDialogCodeButton2:
            purgeGameDB(filename , RomPath);
            break;
        case kDialogCodeButton3:
            m_RemoveAll = true;
            purgeGameDB(filename , RomPath);
            break;
    };

}

void updateDisplayRom(QString romname, int display, QString Systemname)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE gamemetadata SET display = :DISPLAY "
                  "WHERE romname = :ROMNAME AND system = :SYSTEM");
    
    query.bindValue(":DISPLAY", display);
    query.bindValue(":ROMNAME", romname);
    query.bindValue(":SYSTEM", Systemname);

    if (!query.exec())
        MythDB::DBError("updateDisplayRom", query);

}

void updateDiskCount(QString romname, int diskcount, QString GameType)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE gamemetadata SET diskcount = :DISKCOUNT "
                  "WHERE romname = :ROMNAME AND gametype = :GAMETYPE ");

    query.bindValue(":DISKCOUNT",diskcount);
    query.bindValue(":ROMNAME", romname);
    query.bindValue(":GAMETYPE",GameType);

    if (!query.exec())
        MythDB::DBError("updateDiskCount", query);

}

void updateGameName(QString romname, QString GameName, QString Systemname)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE gamemetadata SET GameName = :GAMENAME "
                  "WHERE romname = :ROMNAME AND system = :SYSTEM ");

    query.bindValue(":GAMENAME", GameName);
    query.bindValue(":ROMNAME", romname);
    query.bindValue(":SYSTEM", Systemname);

    if (!query.exec())
        MythDB::DBError("updateGameName", query);

}


static void UpdateGameCounts(QStringList updatelist)
{
    MSqlQuery query(MSqlQuery::InitCon());

    QRegExp multiDiskRGXP = QRegExp( "[0-4]$", Qt::CaseSensitive, QRegExp::RegExp);
    int diskcount = 0;
    int pos = 0;

    QString lastrom, firstname, basename;

    for ( QStringList::Iterator it = updatelist.begin(); it != updatelist.end(); ++it ) 
    {
        diskcount = 0;
        QString GameType = *it;
        VERBOSE(VB_GENERAL, LOC + QString("Update gametype %1").arg(GameType));

        query.prepare("SELECT romname,system,spandisks,gamename FROM "
              "gamemetadata,gameplayers WHERE "
              "gamemetadata.gametype = :GAMETYPE AND "
              "playername = system ORDER BY romname");

        query.bindValue(":GAMETYPE",GameType);

        if (query.exec())
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
                    pos = RomName.lastIndexOf(".");
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

    MythProgressDialog *progressDlg =
        new MythProgressDialog(
            QObject::tr("Updating %1(%2) ROM database")
            .arg(handler->SystemName()).arg(handler->GameType()),
            m_GameMap.size());

    GameScanMap::Iterator iter;

    QString GameName, Genre, Country, CRC32, Year, Plot;
    QString Publisher, Version, Fanart, Boxart, ScreenShot;
    QString thequery, queryvalues;

    int removalprompt = gCoreContext->GetSetting("GameRemovalPrompt").toInt();
    int indepth = gCoreContext->GetSetting("GameDeepScan").toInt();
    QString screenShotPath = gCoreContext->GetSetting("mythgame.screenshotdir");

    for (iter = m_GameMap.begin(); iter != m_GameMap.end(); ++iter)
    {

        if (iter.value().FoundLoc() == inFileSystem)
        {
            if (indepth)
            {
                GetMetadata(handler, iter.value().RomFullPath(), &Genre, &Year, &Country, &CRC32, &GameName,
                            &Plot, &Publisher, &Version, &Fanart, &Boxart);
            }
            else
            {
                Genre = QObject::tr("Unknown") + handler->GameType();
                Country = QObject::tr("Unknown");
                CRC32.clear();
                Year = QObject::tr("19xx");
                GameName = QObject::tr("Unknown");
                Plot = QObject::tr("Unknown");
                Publisher = QObject::tr("Unknown");
                Version = QObject::tr("0");
                Fanart.clear();
                Boxart.clear();
            }

            if (GameName == QObject::tr("Unknown")) 
                GameName = iter.value().GameName();

            int suffixPos = iter.value().Rom().lastIndexOf(QChar('.'));
            QString baseName = iter.value().Rom();

            if (suffixPos > 0) 
                baseName = iter.value().Rom().left(suffixPos); 

            baseName = screenShotPath + "/" + baseName;

            if (QFile(baseName + ".png").exists())
                ScreenShot = baseName + ".png";
            else if (QFile(baseName + ".jpg").exists())
                ScreenShot = baseName + ".jpg";
            else if (QFile(baseName + ".gif").exists())
                ScreenShot = baseName + ".gif";
            else
                ScreenShot.clear();

            //VERBOSE(VB_GENERAL, QString("file %1 - genre %2 ").arg(iter.data().Rom()).arg(Genre));
            //VERBOSE(VB_GENERAL, QString("screenshot %1").arg(ScreenShot));

            query.prepare("INSERT INTO gamemetadata "
                          "(system, romname, gamename, genre, year, gametype, " 
                          "rompath, country, crc_value, diskcount, display, plot, "
                          "publisher, version, fanart, boxart, screenshot) "
                          "VALUES (:SYSTEM, :ROMNAME, :GAMENAME, :GENRE, :YEAR, "
                          ":GAMETYPE, :ROMPATH, :COUNTRY, :CRC32, '1', '1', :PLOT, :PUBLISHER, :VERSION, "
                          ":FANART, :BOXART, :SCREENSHOT)");

            query.bindValue(":SYSTEM",handler->SystemName());
            query.bindValue(":ROMNAME",iter.value().Rom());
            query.bindValue(":GAMENAME",GameName);
            query.bindValue(":GENRE",Genre);
            query.bindValue(":YEAR",Year);
            query.bindValue(":GAMETYPE",handler->GameType());
            query.bindValue(":ROMPATH",iter.value().RomPath());
            query.bindValue(":COUNTRY",Country);
            query.bindValue(":CRC32", CRC32);
            query.bindValue(":PLOT", Plot);
            query.bindValue(":PUBLISHER", Publisher);
            query.bindValue(":VERSION", Version);
            query.bindValue(":FANART", Fanart);
            query.bindValue(":BOXART", Boxart);
            query.bindValue(":SCREENSHOT", ScreenShot);

            if (!query.exec()) 
                MythDB::DBError("GameHandler::UpdateGameDB - " 
                                "insert gamemetadata", query); 
        } 
        else if ((iter.value().FoundLoc() == inDatabase) && (removalprompt))
        {

            promptForRemoval( iter.value().Rom() , iter.value().RomPath() );
        }

        progressDlg->setProgress(++counter);
    }

    progressDlg->Close();
    progressDlg->deleteLater();
}

void GameHandler::VerifyGameDB(GameHandler *handler)
{
    int counter = 0;
    GameScanMap::Iterator iter;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT romname,rompath,gamename FROM gamemetadata "
                  "WHERE system = :SYSTEM");

    query.bindValue(":SYSTEM",handler->SystemName());

    if (!query.exec())
        MythDB::DBError("GameHandler::VerifyGameDB - "
                        "select", query);

    MythProgressDialog *progressDlg = new MythProgressDialog(
        QObject::tr("Verifying %1 files").arg(handler->SystemName()),
        query.size());

    // For every file we know about, check to see if it still exists.
    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            QString RomName = query.value(0).toString();
            QString RomPath = query.value(1).toString();
            QString GameName = query.value(2).toString();
            if (RomName != QString::null)
            {
                if ((iter = m_GameMap.find(RomName)) != m_GameMap.end())
                {
                    // If it's both on disk and in the database we're done with it.
                    m_GameMap.erase(iter);
                }
                else
                {
                    // If it's only in the database add it to our list and mark it for
                    // removal.
                    m_GameMap[RomName] = GameScan(RomName,RomPath + "/" + RomName,inDatabase,
                                         GameName,RomPath);
                }
            }
            progressDlg->setProgress(++counter);
        }
    }
    progressDlg->Close();
    progressDlg->deleteLater();
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

    QFileInfoList List = RomDir.entryInfoList();
    for (QFileInfoList::const_iterator it = List.begin();
         it != List.end(); ++it)
    {
        QFileInfo Info = *it;
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

                r.setPattern("^" + Info.suffix() + "$");
                r.setCaseSensitivity(Qt::CaseInsensitive);
                QStringList result;
                for (int x = 0; x < handler->validextensions.size(); x++)
                {
                    QString extension = handler->validextensions.at(x);
                    if (extension.contains(r))
                        result.append(extension);
                }
                if (result.isEmpty()) 
                    continue;
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

    DialogCode result = MythPopupBox::ShowButtonPopup(
        GetMythMainWindow(),
        QObject::tr("Are you sure?"),
        QString(QObject::tr("This will clear all Game Meta Data\n"
                            "from the database. Are you sure you\n"
                            "want to do this?" )),
        buttonText, kDialogCodeButton0);

    switch (result)
    {
        case kDialogCodeRejected:
        case kDialogCodeButton0:
        default:
            // Do Nothing
            break;
        case kDialogCodeButton1:
            MSqlQuery query(MSqlQuery::InitCon());
            if (!query.exec("DELETE FROM gamemetadata;"))
                MythDB::DBError("GameHandler::clearAllGameData - "
                                "delete gamemetadata", query);
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
    QFileInfoList List = RomDir.entryInfoList();
    for (QFileInfoList::const_iterator it = List.begin();
         it != List.end(); ++it)
    {
        QFileInfo Info = *it;
        QString RomName = Info.fileName();
        QString GameName = Info.completeBaseName();

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

                r.setPattern("^" + Info.suffix() + "$");
                r.setCaseSensitivity(Qt::CaseInsensitive);
                QStringList result;
                for (int x = 0; x < handler->validextensions.size(); x++)
                {
                    QString extension = handler->validextensions.at(x);
                    if (extension.contains(r))
                        result.append(extension);
                }

                if (result.isEmpty()) 
                    continue;
            }

            m_GameMap[RomName] = GameScan(RomName,Info.filePath(),inFileSystem,
                                 GameName, Info.absoluteDir().path());

            VERBOSE(VB_GENERAL, LOC + QString("Found Rom : (%1) - %2")
                    .arg(handler->SystemName()).arg(RomName));

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

    if ((!handler->SystemRomPath().isEmpty()) && (handler->GameType() != "PC"))
    {
        QDir d(handler->SystemRomPath());
        if (d.exists())
            maxcount = buildFileCount(handler->SystemRomPath(),handler);
        else 
        {
            VERBOSE(VB_GENERAL, LOC_ERR + QString("Rom Path does not exist: %1")
                              .arg(handler->SystemRomPath()));
            return;
        }
    }
    else
        maxcount = 100;

    MythProgressDialog *pdial = new MythProgressDialog(
        QObject::tr("Scanning for %1 game(s)...").arg(handler->SystemName()),
        maxcount);
    pdial->setProgress(0);

    if (handler->GameType() == "PC") 
    {
        m_GameMap[handler->SystemCmdLine()] = 
                GameScan(handler->SystemCmdLine(),
                    handler->SystemCmdLine(),
                    inFileSystem,
                    handler->SystemName(),
                    handler->SystemCmdLine().left(handler->SystemCmdLine().lastIndexOf(QRegExp("/"))));


        pdial->setProgress(maxcount);
        VERBOSE(VB_GENERAL, LOC + QString("PC Game %1").arg(handler->SystemName()));

    }
    else
    {   
        int filecount = 0;
        buildFileList(handler->SystemRomPath(), handler, pdial, &filecount);
    }

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


    pdial->Close();
    pdial->deleteLater();
}

void GameHandler::processAllGames(void)
{
    checkHandlers();
    QStringList updatelist;

    for (int x = 0; x < handlers->size(); x++)
    {
        GameHandler *handler = handlers->at(x);

        if (handler)
        {
            updateSettings(handler);
            handler->processGames(handler);

            if (handler->needRebuild())
                updatelist.append(handler->GameType());
        }
    }

    if (!updatelist.isEmpty()) 
        UpdateGameCounts(updatelist);
}

GameHandler* GameHandler::GetHandler(RomInfo *rominfo)
{
    if (!rominfo)
        return NULL;

    for (int x = 0; x < handlers->size(); x++)
    {
        GameHandler *handler = handlers->at(x);
        if (handler)
        {
            if (rominfo->System() == handler->SystemName())
                return handler;
        }
    }

    return NULL;
}

GameHandler* GameHandler::GetHandlerByName(QString systemname)
{
    if (systemname.isEmpty() || systemname.isNull())
        return NULL;

    for (int x = 0; x < handlers->size(); x++)
    {
        GameHandler *handler = handlers->at(x);

        if (handler)
        {
            if (handler->SystemName() == systemname)
                return handler;
        }
    }

    return NULL;
}

void GameHandler::Launchgame(RomInfo *romdata, QString systemname)
{
    GameHandler *handler;

    if (!systemname.isEmpty() && !systemname.isNull()) 
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
                QRegExp rxp = QRegExp( "%d[0-4]", Qt::CaseSensitive, QRegExp::RegExp);

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

    QString savedir = QDir::current().path();
    QDir d;
    if (!handler->SystemWorkingPath().isEmpty()) 
    {
        if (!d.cd(handler->SystemWorkingPath()))
        {
            VERBOSE(VB_GENERAL, LOC_ERR + QString("Failed to change to specified Working Directory: %1")
                    .arg(handler->SystemWorkingPath()));
        }
    }
    VERBOSE(VB_GENERAL, LOC + QString("Launching Game : %1 : %2")
           .arg(handler->SystemName())
           .arg(exec));


    QStringList cmdlist = exec.split(";");
    if (cmdlist.count() > 0)
    {
        for (QStringList::Iterator cmd = cmdlist.begin(); cmd != cmdlist.end();
             ++cmd )
        {
            VERBOSE(VB_GENERAL, LOC + QString("Executing : %1").arg(*cmd));
            myth_system(*cmd);
        }
    }
    else
    {
        VERBOSE(VB_GENERAL, LOC + QString("Executing : %1").arg(exec));
        myth_system(exec);
    }

    (void)d.cd(savedir);      
}

RomInfo *GameHandler::CreateRomInfo(RomInfo *parent)
{
    if (!parent || !GetHandler(parent))
        return NULL;

    return new RomInfo(*parent);
}

void GameHandler::registerHandler(GameHandler *handler)
{
    handlers->append(handler);
}

