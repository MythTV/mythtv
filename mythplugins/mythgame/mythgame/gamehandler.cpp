
#include "gamehandler.h"
#include "rominfo.h"
#include "rom_metadata.h"

#include <QRegExp>
#include <QDir>
#include <QList>

#include <mythdb.h>
#include <mythdbcon.h>
#include <mythsystemlegacy.h>
#include <mythcontext.h>
#include <mythdialogs.h>
#include <mythuihelper.h>
#include <mythdialogbox.h>
#include <mythmainwindow.h>
#include <mythprogressdialog.h>

#define LOC_ERR QString("MythGame:GAMEHANDLER Error: ")
#define LOC QString("MythGame:GAMEHANDLER: ")

static QList<GameHandler*> *handlers = NULL;

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
                                        .remove(" ").split(",", QString::SkipEmptyParts);
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
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("No romDB data read from "
            "database for gametype %1 . Not imported?").arg(GameType));
    else
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Loaded %1 items from romDB Database") .arg(romDB.count()));
}

void GameHandler::GetMetadata(GameHandler *handler, QString rom, QString* Genre, QString* Year,
                              QString* Country, QString* CRC32, QString* GameName,
                              QString *Plot, QString *Publisher, QString *Version,
                              QString* Fanart, QString* Boxart)
{
    QString key;
    QString tmpcrc;

    *CRC32 = crcinfo(rom, handler->GameType(), &key, &romDB);

#if 0
    LOG(VB_GENERAL, LOG_DEBUG, "Key = " + key);
#endif

    // Set our default values
    *Year = tr("19xx", "Default game year");
    *Country = tr("Unknown", "Unknown country");
    *GameName = tr("Unknown", "Unknown game name");
    *Genre = tr("Unknown", "Unknown genre");
    *Plot = tr("Unknown", "Unknown plot");
    *Publisher = tr("Unknown", "Unknown publisher");
    *Version = tr("0", "Default game version");
    (*Fanart).clear();
    (*Boxart).clear();

    if (!(*CRC32).isEmpty())
    {
        if (romDB.contains(key))
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("ROMDB FOUND for %1 - %2")
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
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("NO ROMDB FOUND for %1 (%2)")
                    .arg(rom).arg(*CRC32));
        }

    };

    if ((*Genre == tr("Unknown", "Unknown genre")) || (*Genre).isEmpty())
        *Genre = tr("Unknown %1", "Unknown genre")
            .arg( handler->GameType() );

}

static void purgeGameDB(QString filename, QString RomPath)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Purging %1 - %2").arg(RomPath)
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

void GameHandler::promptForRemoval(GameScan scan)
{
    QString filename = scan.Rom();
    QString RomPath = scan.RomFullPath();

    if (m_RemoveAll)
        purgeGameDB(filename , RomPath);

    if (m_KeepAll || m_RemoveAll)
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythDialogBox *removalPopup = new MythDialogBox(
        //: %1 is the file name
        tr("%1 appears to be missing.\n"
           "Remove it from the database?")
            .arg(filename), popupStack, "chooseSystemPopup");

    if (removalPopup->Create())
    {
        removalPopup->SetReturnEvent(this, "removalPopup");

        removalPopup->AddButton(tr("No"));
        removalPopup->AddButton(tr("No to all"));
        removalPopup->AddButton(tr("Yes"), qVariantFromValue(scan));
        removalPopup->AddButton(tr("Yes to all"), qVariantFromValue(scan));
        popupStack->AddScreen(removalPopup);
}
    else
        delete removalPopup;
}

static void updateDisplayRom(QString romname, int display, QString Systemname)
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

static void updateDiskCount(QString romname, int diskcount, QString GameType)
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

static void updateGameName(QString romname, QString GameName, QString Systemname)
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
        LOG(VB_GENERAL, LOG_NOTICE,
            LOC + QString("Update gametype %1").arg(GameType));

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

                basename = RomName;

                if (spandisks)
                {
                    int extlength = 0;
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

    //: %1 is the system name, %2 is the game type
    QString message = tr("Updating %1 (%2) ROM database")
                          .arg(handler->SystemName())
                          .arg(handler->GameType());

    CreateProgress(message);

    if (m_progressDlg)
        m_progressDlg->SetTotal(m_GameMap.size());

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
                /*: %1 is the game type, when we don't know the genre we use the
                 * game type */
                Genre = tr("Unknown %1", "Unknown genre").arg(handler->GameType());
                Country = tr("Unknown", "Unknown country");
                CRC32.clear();
                Year = tr("19xx", "Default game year");
                GameName = tr("Unknown", "Unknown game name");
                Plot = tr("Unknown", "Unknown plot");
                Publisher = tr("Unknown", "Unknown publisher");
                Version = tr("0", "Default game version");
                Fanart.clear();
                Boxart.clear();
            }

            if (GameName == tr("Unknown", "Unknown game name"))
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

#if 0
            LOG(VB_GENERAL, LOG_INFO, QString("file %1 - genre %2 ")
                    .arg(iter.data().Rom()).arg(Genre));
            LOG(VB_GENERAL, LOG_INFO, QString("screenshot %1").arg(ScreenShot));
#endif

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

            promptForRemoval( iter.value() );
        }

        if (m_progressDlg)
            m_progressDlg->SetProgress(++counter);
    }

    if (m_progressDlg)
    {
        m_progressDlg->Close();
        m_progressDlg = NULL;
}
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

    //: %1 is the system name
    QString message = tr("Verifying %1 files...").arg(handler->SystemName());

    CreateProgress(message);

    if (m_progressDlg)
        m_progressDlg->SetTotal(query.size());

    // For every file we know about, check to see if it still exists.
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
        if (m_progressDlg)
            m_progressDlg->SetProgress(++counter);
    }

    if (m_progressDlg)
    {
        m_progressDlg->Close();
        m_progressDlg = NULL;
    }
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

    RomDir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    QFileInfoList List = RomDir.entryInfoList();
    for (QFileInfoList::const_iterator it = List.begin();
         it != List.end(); ++it)
    {
        QFileInfo Info = *it;
        QString RomName = Info.fileName();

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
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythDialogBox *clearPopup = new MythDialogBox(
        tr("This will clear all game metadata from the database. Are you sure "
            "you want to do this?"), popupStack, "clearAllPopup");

    if (clearPopup->Create())
    {
        clearPopup->SetReturnEvent(this, "clearAllPopup");
        clearPopup->AddButton(tr("No"));
        clearPopup->AddButton(tr("Yes"));
        popupStack->AddScreen(clearPopup);
    }
    else
        delete clearPopup;
}

void GameHandler::buildFileList(QString directory, GameHandler *handler,
                                int* filecount)
{
    QDir RomDir(directory);

    // If we can't read its contents move on
    if (!RomDir.isReadable())
        return;

    RomDir.setSorting( QDir:: DirsFirst | QDir::Name );
    RomDir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    QFileInfoList List = RomDir.entryInfoList();
    for (QFileInfoList::const_iterator it = List.begin();
         it != List.end(); ++it)
    {
        QFileInfo Info = *it;
        QString RomName = Info.fileName();
        QString GameName = Info.completeBaseName();

        if (Info.isDir())
        {
            buildFileList(Info.filePath(), handler, filecount);
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

            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Found ROM : (%1) - %2")
                    .arg(handler->SystemName()).arg(RomName));

            *filecount = *filecount + 1;
            if (m_progressDlg)
                m_progressDlg->SetProgress(*filecount);

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
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("ROM Path does not exist: %1")
                    .arg(handler->SystemRomPath()));
            return;
        }
    }
    else
        maxcount = 100;

    if (handler->GameType() == "PC")
    {
        MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

        //: %1 is the system name
        QString message = tr("Scanning for %1 games...")
            .arg(handler->SystemName());
        MythUIBusyDialog *busyDialog = new MythUIBusyDialog(message, popupStack,
                                                "gamescanbusy");

        if (busyDialog->Create())
            popupStack->AddScreen(busyDialog, false);
        else
        {
            delete busyDialog;
            busyDialog = NULL;
        }

        m_GameMap[handler->SystemCmdLine()] =
                GameScan(handler->SystemCmdLine(),
                    handler->SystemCmdLine(),
                    inFileSystem,
                    handler->SystemName(),
                    handler->SystemCmdLine().left(handler->SystemCmdLine().lastIndexOf(QRegExp("/"))));

        if (busyDialog)
            busyDialog->Close();

        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("PC Game %1").arg(handler->SystemName()));
    }
    else
    {
        QString message = tr("Scanning for %1 games...")
            .arg(handler->SystemName());
        CreateProgress(message);

        if (m_progressDlg)
            m_progressDlg->SetTotal(maxcount);

        int filecount = 0;
        buildFileList(handler->SystemRomPath(), handler, &filecount);

        if (m_progressDlg)
        {
            m_progressDlg->Close();
            m_progressDlg = NULL;
        }
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

    if (exec.isEmpty())
        return;

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
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Failed to change to specified Working Directory: %1")
                    .arg(handler->SystemWorkingPath()));
        }
    }
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Launching Game : %1 : %2")
           .arg(handler->SystemName())
           .arg(exec));

    GetMythUI()->AddCurrentLocation(QString("MythGame %1 ( %2 )").arg(handler->SystemName()).arg(exec));

    QStringList cmdlist = exec.split(";");
    if (cmdlist.count() > 0)
    {
        for (QStringList::Iterator cmd = cmdlist.begin(); cmd != cmdlist.end();
             ++cmd )
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Executing : %1").arg(*cmd));
            myth_system(*cmd, kMSProcessEvents);
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Executing : %1").arg(exec));
        myth_system(exec, kMSProcessEvents);
    }

    GetMythUI()->RemoveCurrentLocation();

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

void GameHandler::customEvent(QEvent *event)
{
    if (event->type() == DialogCompletionEvent::kEventType)
    {
        DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);

        QString resultid   = dce->GetId();
        QString resulttext = dce->GetResultText();

        if (resultid == "removalPopup")
        {
            int buttonNum = dce->GetResult();
            GameScan scan = qVariantValue<GameScan>(dce->GetData());
            switch (buttonNum)
            {
                case 1:
                    m_KeepAll = true;
                    break;
                case 2:
                    purgeGameDB(scan.Rom() , scan.RomFullPath());
                    break;
                case 3:
                    m_RemoveAll = true;
                    purgeGameDB(scan.Rom() , scan.RomFullPath());
                    break;
                default:
                    break;
            };
        }
        else if (resultid == "clearAllPopup")
        {
            int buttonNum = dce->GetResult();
            switch (buttonNum)
            {
                case 1:
                    clearAllMetadata();
                    break;
                default:
                    break;
            }
        }
    }
}

void GameHandler::clearAllMetadata(void)
{
     MSqlQuery query(MSqlQuery::InitCon());
     if (!query.exec("DELETE FROM gamemetadata;"))
         MythDB::DBError("GameHandler::clearAllGameData - "
                          "delete gamemetadata", query);
}

void GameHandler::CreateProgress(QString message)
{
    if (m_progressDlg)
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

    m_progressDlg = new MythUIProgressDialog(message, popupStack,
                                             "gameprogress");

    if (m_progressDlg->Create())
    {
        popupStack->AddScreen(m_progressDlg, false);
    }
    else
    {
        delete m_progressDlg;
        m_progressDlg = NULL;
    }
}
