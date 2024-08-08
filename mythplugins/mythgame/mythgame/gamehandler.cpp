// C++
#include <utility>

// Qt
#include <QDir>
#include <QList>
#include <QRegularExpression>

// MythTV
#include <libmyth/mythcontext.h>
#include <libmythbase/mythdb.h>
#include <libmythbase/mythdbcon.h>
#include <libmythbase/mythsystemlegacy.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythprogressdialog.h>
#include <libmythui/mythuihelper.h>

// MythGame
#include "gamehandler.h"
#include "rominfo.h"
#include "rom_metadata.h"

#define LOC_ERR QString("MythGame:GAMEHANDLER Error: ")
#define LOC QString("MythGame:GAMEHANDLER: ")

static QList<GameHandler*> *handlers = nullptr;

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
        handler->m_rompath = query.value(0).toString();
        handler->m_workingpath = query.value(1).toString();
        handler->m_commandline = query.value(2).toString();
        handler->m_screenshots = query.value(3).toString();
        handler->m_gameplayerid = query.value(4).toInt();
        handler->m_gametype = query.value(5).toString();
        handler->m_validextensions = query.value(6).toString().trimmed()
                                        .remove(" ").split(",", Qt::SkipEmptyParts);
        handler->m_spandisks = query.value(7).toBool();
    }
}

GameHandler* GameHandler::s_newInstance = nullptr;

GameHandler* GameHandler::newHandler(QString name)
{
    s_newInstance = new GameHandler();
    s_newInstance->m_systemname = std::move(name);

    updateSettings(s_newInstance);

    return s_newInstance;
}

// Creates/rebuilds the handler list and then returns the count.
uint GameHandler::count(void)
{
    checkHandlers();
    return handlers->count();
}

void GameHandler::InitMetaDataMap(const QString& GameType)
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
                  .arg(query.value(0).toString(),
                       query.value(9).toString());
            m_romDB[key] = RomData(
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

    if (m_romDB.count() == 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("No romDB data read from "
            "database for gametype %1 . Not imported?").arg(GameType));
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Loaded %1 items from romDB Database") .arg(m_romDB.count()));
    }
}

void GameHandler::GetMetadata(GameHandler *handler, const QString& rom, QString* Genre, QString* Year,
                              QString* Country, QString* CRC32, QString* GameName,
                              QString *Plot, QString *Publisher, QString *Version,
                              QString* Fanart, QString* Boxart)
{
    QString key;

    *CRC32 = crcinfo(rom, handler->GameType(), &key, &m_romDB);

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
        if (m_romDB.contains(key))
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("ROMDB FOUND for %1 - %2")
                     .arg(m_romDB[key].GameName(), key));
            *Year      = m_romDB[key].Year();
            *Country   = m_romDB[key].Country();
            *Genre     = m_romDB[key].Genre();
            *Publisher = m_romDB[key].Publisher();
            *GameName  = m_romDB[key].GameName();
            *Version   = m_romDB[key].Version();
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("NO ROMDB FOUND for %1 (%2)")
                    .arg(rom, *CRC32));
        }

    };

    if ((*Genre == tr("Unknown", "Unknown genre")) || (*Genre).isEmpty())
        *Genre = tr("Unknown %1", "Unknown genre")
            .arg( handler->GameType() );

}

static void purgeGameDB(const QString& filename, const QString& RomPath)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Purging %1 - %2")
            .arg(RomPath, filename));

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

void GameHandler::promptForRemoval(const GameScan& scan)
{
    QString filename = scan.Rom();
    QString RomPath = scan.RomFullPath();

    if (m_removeAll)
        purgeGameDB(filename , RomPath);

    if (m_keepAll || m_removeAll)
        return;

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *removalPopup = new MythDialogBox(
        //: %1 is the file name
        tr("%1 appears to be missing.\n"
           "Remove it from the database?")
            .arg(filename), popupStack, "chooseSystemPopup");

    if (removalPopup->Create())
    {
        removalPopup->SetReturnEvent(this, "removalPopup");

        removalPopup->AddButton(tr("No"));
        removalPopup->AddButton(tr("No to all"));
        removalPopup->AddButtonV(tr("Yes"), QVariant::fromValue(scan));
        removalPopup->AddButtonV(tr("Yes to all"), QVariant::fromValue(scan));
        popupStack->AddScreen(removalPopup);
}
    else
    {
        delete removalPopup;
    }
}

static void updateDisplayRom(const QString& romname, int display, const QString& Systemname)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE gamemetadata SET display = :DISPLAY "
                  "WHERE romname = :ROMNAME AND `system` = :SYSTEM");

    query.bindValue(":DISPLAY", display);
    query.bindValue(":ROMNAME", romname);
    query.bindValue(":SYSTEM", Systemname);

    if (!query.exec())
        MythDB::DBError("updateDisplayRom", query);

}

static void updateDiskCount(const QString& romname, int diskcount, const QString& GameType)
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

static void updateGameName(const QString& romname, const QString& GameName, const QString& Systemname)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE gamemetadata SET GameName = :GAMENAME "
                  "WHERE romname = :ROMNAME AND `system` = :SYSTEM ");

    query.bindValue(":GAMENAME", GameName);
    query.bindValue(":ROMNAME", romname);
    query.bindValue(":SYSTEM", Systemname);

    if (!query.exec())
        MythDB::DBError("updateGameName", query);

}


static void UpdateGameCounts(const QStringList& updatelist)
{
    MSqlQuery query(MSqlQuery::InitCon());

    static const QRegularExpression multiDiskRGXP { "[0-4]$" };

    QString lastrom;
    QString firstname;
    QString basename;

    for (const auto & GameType : std::as_const(updatelist))
    {
        LOG(VB_GENERAL, LOG_NOTICE,
            LOC + QString("Update gametype %1").arg(GameType));

        query.prepare("SELECT romname,`system`,spandisks,gamename FROM "
              "gamemetadata,gameplayers WHERE "
              "gamemetadata.gametype = :GAMETYPE AND "
              "playername = `system` ORDER BY romname");

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
                    int diskcount = 0;
                    int extlength = 0;
                    int pos = RomName.lastIndexOf(".");
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
                    {
                        basename = GameName;
                    }

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
        .arg(handler->SystemName(), handler->GameType());

    CreateProgress(message);

    if (m_progressDlg)
        m_progressDlg->SetTotal(m_gameMap.size());

    QString GameName;
    QString Genre;
    QString Country;
    QString CRC32;
    QString Year;
    QString Plot;
    QString Publisher;
    QString Version;
    QString Fanart;
    QString Boxart;
    QString ScreenShot;

    int removalprompt = gCoreContext->GetSetting("GameRemovalPrompt").toInt();
    int indepth = gCoreContext->GetSetting("GameDeepScan").toInt();
    QString screenShotPath = gCoreContext->GetSetting("mythgame.screenshotdir");

    for (const auto & game : std::as_const(m_gameMap))
    {

        if (game.FoundLoc() == inFileSystem)
        {
            if (indepth)
            {
                GetMetadata(handler, game.RomFullPath(), &Genre, &Year, &Country, &CRC32, &GameName,
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
                GameName = game.GameName();

            int suffixPos = game.Rom().lastIndexOf(QChar('.'));
            QString baseName = game.Rom();

            if (suffixPos > 0)
                baseName = game.Rom().left(suffixPos);

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
                          "(`system`, romname, gamename, genre, year, gametype, "
                          "rompath, country, crc_value, diskcount, display, plot, "
                          "publisher, version, fanart, boxart, screenshot) "
                          "VALUES (:SYSTEM, :ROMNAME, :GAMENAME, :GENRE, :YEAR, "
                          ":GAMETYPE, :ROMPATH, :COUNTRY, :CRC32, '1', '1', :PLOT, :PUBLISHER, :VERSION, "
                          ":FANART, :BOXART, :SCREENSHOT)");

            query.bindValueNoNull(":SYSTEM",handler->SystemName());
            query.bindValueNoNull(":ROMNAME",game.Rom());
            query.bindValueNoNull(":GAMENAME",GameName);
            query.bindValueNoNull(":GENRE",Genre);
            query.bindValueNoNull(":YEAR",Year);
            query.bindValueNoNull(":GAMETYPE",handler->GameType());
            query.bindValueNoNull(":ROMPATH",game.RomPath());
            query.bindValueNoNull(":COUNTRY",Country);
            query.bindValueNoNull(":CRC32", CRC32);
            query.bindValueNoNull(":PLOT", Plot);
            query.bindValueNoNull(":PUBLISHER", Publisher);
            query.bindValueNoNull(":VERSION", Version);
            query.bindValueNoNull(":FANART", Fanart);
            query.bindValueNoNull(":BOXART", Boxart);
            query.bindValueNoNull(":SCREENSHOT", ScreenShot);

            if (!query.exec())
                MythDB::DBError("GameHandler::UpdateGameDB - "
                                "insert gamemetadata", query);
        }
        else if ((game.FoundLoc() == inDatabase) && (removalprompt))
        {

            promptForRemoval( game );
        }

        if (m_progressDlg)
            m_progressDlg->SetProgress(++counter);
    }

    if (m_progressDlg)
    {
        m_progressDlg->Close();
        m_progressDlg = nullptr;
}
}

void GameHandler::VerifyGameDB(GameHandler *handler)
{
    int counter = 0;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT romname,rompath,gamename FROM gamemetadata "
                  "WHERE `system` = :SYSTEM");

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
        if (!RomName.isEmpty())
        {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
            auto iter = m_gameMap.find(RomName);
#else
            auto iter = m_gameMap.constFind(RomName);
#endif
            if (iter != m_gameMap.end())
            {
                // If it's both on disk and in the database we're done with it.
                m_gameMap.erase(iter);
            }
            else
            {
                // If it's only in the database add it to our list and mark it for
                // removal.
                m_gameMap[RomName] = GameScan(RomName,RomPath + "/" + RomName,inDatabase,
                                        GameName,RomPath);
            }
        }
        if (m_progressDlg)
            m_progressDlg->SetProgress(++counter);
    }

    if (m_progressDlg)
    {
        m_progressDlg->Close();
        m_progressDlg = nullptr;
    }
}

// Recurse through the directory and gather a count on how many files there are to process.
// This is used for the progressbar info.
int GameHandler::buildFileCount(const QString& directory, GameHandler *handler)
{
    int filecount = 0;
    QDir RomDir(directory);

    // If we can't read it's contents move on
    if (!RomDir.isReadable())
        return 0;

    RomDir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    QFileInfoList List = RomDir.entryInfoList();
    for (const auto & Info : std::as_const(List))
    {
        if (Info.isDir())
        {
            filecount += buildFileCount(Info.filePath(), handler);
            continue;
        }

        if (handler->m_validextensions.count() > 0)
        {
            QRegularExpression r {
                "^" + Info.suffix() + "$",
                QRegularExpression::CaseInsensitiveOption };
            QStringList result;
            QStringList& exts = handler->m_validextensions;
            std::copy_if(exts.cbegin(), exts.cend(), std::back_inserter(result),
                         [&r](const QString& extension){ return extension.contains(r); } );
            if (result.isEmpty())
                continue;
        }

        filecount++;
    }

    return filecount;
}

void GameHandler::clearAllGameData(void)
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    auto *clearPopup = new MythDialogBox(
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
    {
        delete clearPopup;
    }
}

void GameHandler::buildFileList(const QString& directory, GameHandler *handler,
                                int* filecount)
{
    QDir RomDir(directory);

    // If we can't read its contents move on
    if (!RomDir.isReadable())
        return;

    RomDir.setSorting( QDir:: DirsFirst | QDir::Name );
    RomDir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    QFileInfoList List = RomDir.entryInfoList();
    for (const auto & Info : std::as_const(List))
    {
        QString RomName = Info.fileName();
        QString GameName = Info.completeBaseName();

        if (Info.isDir())
        {
            buildFileList(Info.filePath(), handler, filecount);
            continue;
        }

        if (handler->m_validextensions.count() > 0)
        {
            QRegularExpression r {
                "^" + Info.suffix() + "$",
                QRegularExpression::CaseInsensitiveOption };
            QStringList result;
            QStringList& exts = handler->m_validextensions;
            std::copy_if(exts.cbegin(), exts.cend(), std::back_inserter(result),
                         [&r](const QString& extension){ return extension.contains(r); } );
            if (result.isEmpty())
                continue;
        }

        m_gameMap[RomName] = GameScan(RomName,Info.filePath(),inFileSystem,
                                      GameName, Info.absoluteDir().path());

        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Found ROM : (%1) - %2")
            .arg(handler->SystemName(), RomName));

        *filecount = *filecount + 1;
        if (m_progressDlg)
            m_progressDlg->SetProgress(*filecount);
    }
}

void GameHandler::processGames(GameHandler *handler)
{
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
    {
        maxcount = 100;
    }

    if (handler->GameType() == "PC")
    {
        MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

        //: %1 is the system name
        QString message = tr("Scanning for %1 games...")
            .arg(handler->SystemName());
        auto *busyDialog = new MythUIBusyDialog(message, popupStack,
                                                "gamescanbusy");

        if (busyDialog->Create())
            popupStack->AddScreen(busyDialog, false);
        else
        {
            delete busyDialog;
            busyDialog = nullptr;
        }

        m_gameMap[handler->SystemCmdLine()] =
                GameScan(handler->SystemCmdLine(),
                    handler->SystemCmdLine(),
                    inFileSystem,
                    handler->SystemName(),
                    handler->SystemCmdLine().left(handler->SystemCmdLine().lastIndexOf("/")));

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
            m_progressDlg = nullptr;
        }
    }

    VerifyGameDB(handler);

    // If we still have some games in the list then update the database
    if (!m_gameMap.empty())
    {
        InitMetaDataMap(handler->GameType());

        UpdateGameDB(handler);

        m_romDB.clear();
        handler->setRebuild(true);
    }
    else
    {
        handler->setRebuild(false);
    }
}

void GameHandler::processAllGames(void)
{
    checkHandlers();
    QStringList updatelist;

    for (auto *handler : std::as_const(*handlers))
    {
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
        return nullptr;

    for (auto *handler : std::as_const(*handlers))
    {
        if (handler)
        {
            if (rominfo->System() == handler->SystemName())
                return handler;
        }
    }

    return nullptr;
}

GameHandler* GameHandler::GetHandlerByName(const QString& systemname)
{
    if (systemname.isEmpty())
        return nullptr;

    for (auto *handler : std::as_const(*handlers))
    {
        if (handler)
        {
            if (handler->SystemName() == systemname)
                return handler;
        }
    }

    return nullptr;
}

void GameHandler::Launchgame(RomInfo *romdata, const QString& systemname)
{
    GameHandler *handler = nullptr;

    if (!systemname.isEmpty())
    {
        handler = GetHandlerByName(systemname);
    }
    else
    {
        handler = GetHandler(romdata);
        if (handler == nullptr)
        {
            // Couldn't get handler so abort.
            return;
        }
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
            exec = exec.replace("%s",arg);

            if (handler->SpanDisks())
            {
                static const QRegularExpression rxp { "%d[0-4]" };

                if (exec.contains(rxp))
                {
                    if (romdata->DiskCount() > 1)
                    {
                        // Chop off the extension, .  and last character of the name which we are assuming is the disk #
                        QString basename = romdata->Romname().left(romdata->Romname().length() - (romdata->getExtension().length() + 2));
                        QString extension = romdata->getExtension();
                        QString rom;
                        std::array<QString,7> diskid { "%d0", "%d1", "%d2", "%d3", "%d4", "%d5", "%d6" };

                        for (int disk = 1; disk <= romdata->DiskCount(); disk++)
                        {
                            rom = QString("\"%1/%2%3.%4\"")
                                .arg(romdata->Rompath(), basename,
                                     QString::number(disk), extension);
                            exec = exec.replace(diskid[disk],rom);
                        }
                    } else
                    {   // If there is only one disk make sure we replace %d1 just like %s
                        exec = exec.replace("%d1",arg);
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
           .arg(handler->SystemName(), exec));

    GetMythUI()->AddCurrentLocation(QString("MythGame %1 ( %2 )")
                                    .arg(handler->SystemName(), exec));

    QStringList cmdlist = exec.split(";");
    if (cmdlist.count() > 0)
    {
        for (const auto & cmd : std::as_const(cmdlist))
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Executing : %1").arg(cmd));
            myth_system(cmd, kMSProcessEvents);
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
        return nullptr;

    return new RomInfo(*parent);
}

void GameHandler::registerHandler(GameHandler *handler)
{
    handlers->append(handler);
}

void GameHandler::customEvent(QEvent *event)
{
    if (auto *dce = dynamic_cast<DialogCompletionEvent*>(event))
    {
        QString resultid   = dce->GetId();
//      QString resulttext = dce->GetResultText();

        if (resultid == "removalPopup")
        {
            int buttonNum = dce->GetResult();
            auto scan = dce->GetData().value<GameScan>();
            switch (buttonNum)
            {
                case 1:
                    m_keepAll = true;
                    break;
                case 2:
                    purgeGameDB(scan.Rom() , scan.RomFullPath());
                    break;
                case 3:
                    m_removeAll = true;
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

void GameHandler::CreateProgress(const QString& message)
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
        m_progressDlg = nullptr;
    }
}
