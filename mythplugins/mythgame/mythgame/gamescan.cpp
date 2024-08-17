// C++
#include <utility>

// Qt
#include <QApplication>
#include <QImageReader>
#include <QStringList>
#include <QThread>
#include <QUrl>

// MythTV
#include <libmyth/mythcontext.h>
#include <libmythbase/mythevent.h>
#include <libmythbase/mythlogging.h>
#include <libmythbase/remoteutil.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/mythprogressdialog.h>
#include <libmythui/mythscreenstack.h>

// MythGame
#include "gamescan.h"
#include "gamehandler.h"
#include "rominfo.h"

class MythUIProgressDialog;

GameScannerThread::GameScannerThread(void) :
    MThread("GameScanner"),
    m_hasGUI(gCoreContext->HasGUI())
{
}

void GameScannerThread::run(void)
{
    RunProlog();

    LOG(VB_GENERAL, LOG_INFO, QString("Beginning Game Scan."));

    m_files.clear();
    m_remove.clear();
    m_dbgames = RomInfo::GetAllRomInfo();

    buildFileList();
    verifyFiles();
    updateDB();

    LOG(VB_GENERAL, LOG_INFO, QString("Finished Game Scan."));

    RunEpilog();
}


void GameScannerThread::removeOrphan(const int id)
{
    RomInfo *info = RomInfo::GetRomInfoById(id);
    if (info)
    {
        info->DeleteFromDatabase();
        delete info;
        info = nullptr;
    }
}

void GameScannerThread::verifyFiles()
{
    int counter = 0;

    if (m_hasGUI)
        SendProgressEvent(counter, (uint)m_dbgames.count(),
                          GameScanner::tr("Verifying game files..."));

    // For every file we know about, check to see if it still exists.
    for (const auto *info : std::as_const(m_dbgames))
    {
        QString romfile = info->Romname();
        QString gametype = info->GameType();
        if (!romfile.isEmpty())
        {
            bool found = false;
            // NOLINTNEXTLINE(modernize-loop-convert)
            for (auto p2 = m_files.begin(); p2 != m_files.end(); ++p2)
            {
                if ((*p2).romfile == romfile &&
                    (*p2).gametype == gametype)
                {
                    // We're done here, this file matches one in the DB
                    (*p2).indb = true;
                    found = true;
                    continue;
                }
            }
            if (!found)
            {
                m_remove.append(info->Id());
            }
        }
        if (m_hasGUI)
            SendProgressEvent(++counter);

        delete info;
        info = nullptr;
    }
}

void GameScannerThread::updateDB()
{
    uint counter = 0;
    if (m_hasGUI)
        SendProgressEvent(counter, (uint)(m_files.size() + m_remove.size()),
                          GameScanner::tr("Updating game database..."));

    for (const auto & file : std::as_const(m_files))
    {
        if (!file.indb)
        {
            RomInfo add(0, file.romfile, file.system,
                        file.romname, "", "", false, file.rompath,
                        "", "", 0, file.gametype, 0, "", "", "",
                        "", "", "", "", "");
            add.SaveToDatabase();
            m_dbDataChanged = true;
        }
        if (m_hasGUI)
            SendProgressEvent(++counter);
    }

    for (const uint & p : std::as_const(m_remove))
    {
        removeOrphan(p);
        m_dbDataChanged = true;
    }
}

bool GameScannerThread::buildFileList()
{
    if (m_handlers.empty())
        return false;

    int counter = 0;

    if (m_hasGUI)
        SendProgressEvent(counter, (uint)m_handlers.size(),
                          GameScanner::tr("Searching for games..."));

    for (auto * handler : std::as_const(m_handlers))
    {
        QDir dir(handler->SystemRomPath());
        QStringList extensions = handler->ValidExtensions();
        QStringList filters;
        for (const auto & ext : std::as_const(extensions))
        {
            filters.append(QString("*.%1").arg(ext));
        }

        dir.setNameFilters(filters);
        dir.setFilter(QDir::Files | QDir::Readable | QDir::NoDotAndDotDot);

        QStringList files = dir.entryList();
        for (const auto & file : std::as_const(files))
        {
            RomFileInfo info;
            info.system = handler->SystemName();
            info.gametype = handler->GameType();
            info.romfile = file;
            info.rompath = handler->SystemRomPath();
            info.romname = QFileInfo(file).baseName();
            info.indb = false;
            m_files.append(info);
        }

        if (m_hasGUI)
            SendProgressEvent(++counter);
    }

    return true;
}

void GameScannerThread::SendProgressEvent(uint progress, uint total,
                                          QString message)
{
    if (!m_dialog)
        return;

    auto *pue = new ProgressUpdateEvent(progress, total, std::move(message));
    QApplication::postEvent(m_dialog, pue);
}

GameScanner::GameScanner()
  : m_scanThread(new GameScannerThread())
{
}

GameScanner::~GameScanner()
{
    if (m_scanThread && m_scanThread->wait())
        delete m_scanThread;
}

void GameScanner::doScan(QList<GameHandler*> handlers)
{
    if (m_scanThread->isRunning())
        return;

    if (gCoreContext->HasGUI())
    {
        MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");

        auto *progressDlg = new MythUIProgressDialog("",
                popupStack, "gamescanprogressdialog");

        if (progressDlg->Create())
        {
            popupStack->AddScreen(progressDlg, false);
            connect(m_scanThread->qthread(), &QThread::finished,
                    progressDlg, &MythScreenType::Close);
            connect(m_scanThread->qthread(), &QThread::finished,
                    this, &GameScanner::finishedScan);
        }
        else
        {
            delete progressDlg;
            progressDlg = nullptr;
        }
        m_scanThread->SetProgressDialog(progressDlg);
    }

    m_scanThread->SetHandlers(std::move(handlers));
    m_scanThread->start();
}

void GameScanner::doScanAll()
{
    QList<GameHandler*> hlist;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT DISTINCT playername FROM gameplayers "
                    "WHERE playername <> '';");

    if (!query.exec())
        MythDB::DBError("doScanAll - selecting playername", query);

    while (query.next())
    {
        QString name = query.value(0).toString();
        GameHandler *hnd = GameHandler::GetHandlerByName(name);
        if (hnd)
            hlist.append(hnd);
    }

    doScan(hlist);
}

void GameScanner::finishedScan()
{
    emit finished(m_scanThread->getDataChanged());
}

////////////////////////////////////////////////////////////////////////
