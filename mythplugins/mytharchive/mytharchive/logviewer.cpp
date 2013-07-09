#include <unistd.h>
#include <iostream>
#include <cstdlib>

// qt
#include <QKeyEvent>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>

// mythtv
#include <mythcontext.h>
#include <mythdbcon.h>
#include <mythmainwindow.h>
#include <mythdialogbox.h>
#include <mythuibutton.h>
#include <mythuibuttonlist.h>
#include <mythuitext.h>

// mytharchive
#include "archiveutil.h"
#include "logviewer.h"

const int DEFAULT_UPDATE_TIME = 5;

void showLogViewer(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    QString logDir = getTempDirectory() + "logs";
    QString progressLog;
    QString fullLog;

    // wait for a log file to be available
    int tries = 10;
    while (tries--)
    {
        if (QFile::exists(logDir + "/progress.log"))
            progressLog = logDir + "/progress.log";

        if (QFile::exists(logDir + "/mythburn.log"))
            fullLog = logDir + "/mythburn.log";

        // we wait for both the progress.log and mythburn.log
        if ((!progressLog.isEmpty()) && (!fullLog.isEmpty()))
            break;

        // or we wait for a log from mytharchivehelper
        if (progressLog.isEmpty() && fullLog.isEmpty())
        {
            QStringList logFiles;
            QStringList filters;
            filters << "*.log";

            QDir d(logDir);
            logFiles = d.entryList(filters, QDir::Files | QDir::Readable, QDir::Time);

            if (logFiles.count())
            {
                // the first log file should be the newest one available
                progressLog = logDir + '/' + logFiles[0];
                break;
            }
        }

        sleep(1);
    }

    // do any logs exist?
    if ((!progressLog.isEmpty()) || (!fullLog.isEmpty()))
    {
        LogViewer *viewer = new LogViewer(mainStack);
        viewer->setFilenames(progressLog, fullLog);
        if (viewer->Create())
            mainStack->AddScreen(viewer);
    }
    else
        showWarningDialog(QCoreApplication::translate("LogViewer",
            "Cannot find any logs to show!"));
}

LogViewer::LogViewer(MythScreenStack *parent) :
    MythScreenType(parent, "logviewer"),
    m_autoUpdate(false),
    m_updateTime(DEFAULT_UPDATE_TIME),
    m_updateTimer(NULL),
    m_currentLog(),
    m_progressLog(),
    m_fullLog(),
    m_logList(NULL),
    m_logText(NULL),
    m_exitButton(NULL),
    m_cancelButton(NULL),
    m_updateButton(NULL)
{
    m_updateTime = gCoreContext->GetNumSetting(
        "LogViewerUpdateTime", DEFAULT_UPDATE_TIME);
    m_autoUpdate = gCoreContext->GetNumSetting("LogViewerAutoUpdate", 1);
}

LogViewer::~LogViewer(void)
{
    gCoreContext->SaveSetting("LogViewerUpdateTime", m_updateTime);
    gCoreContext->SaveSetting("LogViewerAutoUpdate", m_autoUpdate ? "1" : "0");

    if (m_updateTimer)
        delete m_updateTimer;
}

bool LogViewer::Create(void)
{
    bool foundtheme = false;

    // Load the theme for this screen
    foundtheme = LoadWindowFromXML("mytharchive-ui.xml", "logviewer", this);

    if (!foundtheme)
        return false;

    bool err = false; 
    UIUtilE::Assign(this, m_logList, "loglist", &err);
    UIUtilE::Assign(this, m_logText, "logitem_text", &err);
    UIUtilE::Assign(this, m_cancelButton, "cancel_button", &err);
    UIUtilE::Assign(this, m_updateButton, "update_button", &err);
    UIUtilE::Assign(this, m_exitButton, "exit_button", &err);

    if (err)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot load screen 'logviewer'");
        return false;
    }

    connect(m_cancelButton, SIGNAL(Clicked()), this, SLOT(cancelClicked()));
    connect(m_updateButton, SIGNAL(Clicked()), this, SLOT(updateClicked()));
    connect(m_exitButton, SIGNAL(Clicked()), this, SLOT(Close()));

    connect(m_logList, SIGNAL(itemSelected(MythUIButtonListItem*)),
            this, SLOT(updateLogItem(MythUIButtonListItem*)));

    m_updateTimer = NULL;
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, SIGNAL(timeout()), SLOT(updateTimerTimeout()) );

    BuildFocusList();

    SetFocusWidget(m_logList);

    return true;
}

void LogViewer::Init(void)
{
    updateClicked();
    if (m_logList->GetCount() > 0)
        m_logList->SetItemCurrent(m_logList->GetCount() - 1);
}

bool LogViewer::keyPressEvent(QKeyEvent *event)
{
    if (GetFocusWidget()->keyPressEvent(event))
         return true;

    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Global", event, actions);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "MENU")
            showMenu();
        else
            handled = false;
    }

    if (!handled && MythScreenType::keyPressEvent(event))
        handled = true;

    return handled;
}

void LogViewer::updateTimerTimeout()
{
    updateClicked();
}

void LogViewer::toggleAutoUpdate(void)
{
    m_autoUpdate = ! m_autoUpdate;

    if (m_autoUpdate)
        m_updateTimer->start(m_updateTime * 1000);
    else
        m_updateTimer->stop();
}

void LogViewer::updateLogItem(MythUIButtonListItem *item)
{
    if (item)
        m_logText->SetText(item->GetText());
}

void LogViewer::cancelClicked(void)
{
    QString tempDir = gCoreContext->GetSetting("MythArchiveTempDir", "");
    QFile lockFile(tempDir + "/logs/mythburncancel.lck");

    if (!lockFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        LOG(VB_GENERAL, LOG_ERR,
            "LogViewer: Failed to create mythburncancel.lck file");

    lockFile.write("Cancel\n\r");
    lockFile.close();

    ShowOkPopup(tr("Background creation has been asked to stop.\n" 
                   "This may take a few minutes."));
}

void LogViewer::updateClicked(void)
{
    m_updateTimer->stop();

    QStringList list;
    loadFile(m_currentLog, list, m_logList->GetCount());

    if (list.size() > 0)
    {
        bool bUpdateCurrent =
                (m_logList->GetCount() == m_logList->GetCurrentPos() + 1) ||
                (m_logList->GetCurrentPos() == 0);

        for (int x = 0; x < list.size(); x++)
            new MythUIButtonListItem(m_logList, list[x]);

        if (bUpdateCurrent)
            m_logList->SetItemCurrent(m_logList->GetCount() - 1);
    }

    bool bRunning = (getSetting("MythArchiveLastRunStatus") == "Running");

    if (!bRunning)
    {
        m_cancelButton->SetEnabled(false);
        m_updateButton->SetEnabled(false);
    }

    if (m_autoUpdate)
    {
        if (m_logList->GetCount() > 0)
            m_updateTimer->start(m_updateTime * 1000);
        else
            m_updateTimer->start(500);
    }
}

QString LogViewer::getSetting(const QString &key)
{
    // read the setting direct from the DB rather than from the settings cache 
    // which isn't aware that the script may have changed something
    MSqlQuery query(MSqlQuery::InitCon());
    if (query.isConnected())
    {
        query.prepare("SELECT data FROM settings WHERE value = :VALUE "
                "AND hostname = :HOSTNAME ;");
        query.bindValue(":VALUE", key);
        query.bindValue(":HOSTNAME", gCoreContext->GetHostName());

        if (query.exec() && query.next())
        {
            return query.value(0).toString();
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, 
            QString("Database not open while trying to load setting: %1")
                .arg(key));
    }

    return QString("");
}

bool LogViewer::loadFile(QString filename, QStringList &list, int startline)
{
    bool strip = !(filename.endsWith("progress.log") || filename.endsWith("mythburn.log"));

    list.clear();

    QFile file(filename);

    if (!file.exists())
        return false;

    if (file.open( QIODevice::ReadOnly ))
    {
        QString s;
        QTextStream stream(&file);

         // ignore the first startline lines
        while ( !stream.atEnd() && startline > 0)
        {
            stream.readLine();
            startline--;
        }

         // read rest of file
        while ( !stream.atEnd() )
        {
            s = stream.readLine();

            if (strip)
            {
                // the logging from mytharchivehelper contains a lot of junk
                // we are not interested in so just strip it out
                int pos = s.indexOf(" - ");
                if (pos != -1)
                    s = s.mid(pos + 3);
            }

            list.append(s);
        }
        file.close();
    }
    else
        return false;

    return true;
}

void LogViewer::setFilenames(const QString &progressLog, const QString &fullLog)
{
    m_progressLog = progressLog;
    m_fullLog = fullLog;
    m_currentLog = progressLog;
}

void LogViewer::showProgressLog(void)
{
    m_currentLog = m_progressLog;
    m_logList->Reset();
    updateClicked();
}

void LogViewer::showFullLog(void)
{
    m_currentLog = m_fullLog;
    m_logList->Reset();
    updateClicked();
}

void LogViewer::showMenu()
{
    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    MythDialogBox *menuPopup = new MythDialogBox(tr("Menu"), popupStack, "actionmenu");

    if (menuPopup->Create())
        popupStack->AddScreen(menuPopup);

    menuPopup->SetReturnEvent(this, "action");

    if (m_autoUpdate)
        menuPopup->AddButton(tr("Don't Auto Update"), SLOT(toggleAutoUpdate()));
    else
        menuPopup->AddButton(tr("Auto Update"), SLOT(toggleAutoUpdate()));

    menuPopup->AddButton(tr("Show Progress Log"), SLOT(showProgressLog()));
    menuPopup->AddButton(tr("Show Full Log"), SLOT(showFullLog()));
}
