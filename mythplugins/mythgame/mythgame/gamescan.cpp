#include <QApplication>
#include <QImageReader>
#include <QStringList>
#include <QThread>
#include <QUrl>
#include <utility>

#include <mythcontext.h>
#include <mythmainwindow.h>
#include <mythscreenstack.h>
#include <mythprogressdialog.h>
#include <mythdialogbox.h>
#include <mythevent.h>
#include <remoteutil.h>
#include <mythlogging.h>

#include "gamescan.h"
#include "gamehandler.h"
#include "rominfo.h"

class MythUIProgressDialog;

GameScannerThread::GameScannerThread(void) :
    MThread("GameScanner"),
    m_HasGUI(gCoreContext->HasGUI())
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

    if (m_HasGUI)
        SendProgressEvent(counter, (uint)m_dbgames.count(),
                          GameScanner::tr("Verifying game files..."));

    // For every file we know about, check to see if it still exists.
    for (auto p1 = m_dbgames.begin(); p1 != m_dbgames.end(); ++p1)
    {
        RomInfo *info = *p1;
        QString romfile = info->Romname();
        QString system = info->System();
        QString gametype = info->GameType();
        if (!romfile.isEmpty())
        {
            bool found = false;
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
        if (m_HasGUI)
            SendProgressEvent(++counter);

        delete info;
        info = nullptr;
    }
}

void GameScannerThread::updateDB()
{
    uint counter = 0;
    if (m_HasGUI)
        SendProgressEvent(counter, (uint)(m_files.size() + m_remove.size()),
                          GameScanner::tr("Updating game database..."));

    for (QList<RomFileInfo>::iterator p = m_files.begin();
                                 p != m_files.end(); ++p)
    {
        if (!(*p).indb)
        {
            RomInfo add(0, (*p).romfile, (*p).system,
                        (*p).romname, "", "", false, (*p).rompath,
                        "", "", 0, (*p).gametype, 0, "", "", "",
                        "", "", "", "", "");
            add.SaveToDatabase();
            m_DBDataChanged = true;
        }
        if (m_HasGUI)
            SendProgressEvent(++counter);
    }

    for (QList<uint>::iterator p = m_remove.begin();
                                 p != m_remove.end(); ++p)
    {
        removeOrphan(*p);
        m_DBDataChanged = true;
    }
}

bool GameScannerThread::buildFileList()
{
    if (m_handlers.empty())
        return false;

    int counter = 0;

    if (m_HasGUI)
        SendProgressEvent(counter, (uint)m_handlers.size(),
                          GameScanner::tr("Searching for games..."));

    for (QList<GameHandler*>::const_iterator iter = m_handlers.begin();
         iter != m_handlers.end(); ++iter)
    {
        QDir dir((*iter)->SystemRomPath());
        QStringList extensions = (*iter)->ValidExtensions();
        QStringList filters;
        for (QStringList::iterator i = extensions.begin();
             i != extensions.end(); ++i)
        {
            filters.append(QString("*.%1").arg(*i));
        }

        dir.setNameFilters(filters);
        dir.setFilter(QDir::Files | QDir::Readable | QDir::NoDotAndDotDot);

        QStringList files = dir.entryList();
        for (QStringList::iterator i = files.begin();
             i != files.end(); ++i)
        {
            RomFileInfo info;
            info.system = (*iter)->SystemName();
            info.gametype = (*iter)->GameType();
            info.romfile = *i;
            info.rompath = (*iter)->SystemRomPath();
            info.romname = QFileInfo(*i).baseName();
            info.indb = false;
            m_files.append(info);
        }

        if (m_HasGUI)
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
{
    m_scanThread = new GameScannerThread();
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
            connect(m_scanThread->qthread(), SIGNAL(finished()),
                    progressDlg, SLOT(Close()));
            connect(m_scanThread->qthread(), SIGNAL(finished()),
                    SLOT(finishedScan()));
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
