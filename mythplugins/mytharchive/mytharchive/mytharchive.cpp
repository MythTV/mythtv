/*
    main.cpp

    MythArchive - mythtv plugin
    
    Starting point for the MythArchive module
*/
#include <csignal>
#include <cstdlib>
#include <iostream>
#ifdef _WIN32
#include <processthreadsapi.h>
#endif

// Qt
#include <QApplication>
#include <QDir>
#include <QTimer>

// mythtv
#include <libmythbase/mythcorecontext.h>
#include <libmythbase/mythlogging.h>
#include <libmythbase/mythplugin.h>
#include <libmythbase/mythpluginapi.h>
#include <libmythbase/mythsystemlegacy.h>
#include <libmythbase/mythversion.h>
#include <libmythui/mythdialogbox.h>
#include <libmythui/mythmainwindow.h>
#include <libmythui/myththemedmenu.h>
#include <libmythui/mythuihelper.h>

// mytharchive
#include "archivedbcheck.h"
#include "archivesettings.h"
#include "archiveutil.h"
#include "exportnative.h"
#include "fileselector.h"
#include "importnative.h"
#include "logviewer.h"
#include "mythburn.h"
#include "recordingselector.h"
#include "selectdestination.h"
#include "videoselector.h"

// return true if the process belonging to the lock file is still running
static bool checkProcess(const QString &lockFile)
{
    // read the PID from the lock file
    QFile file(lockFile);

    bool bOK = file.open(QIODevice::ReadOnly);

    if (!bOK)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Unable to open file %1").arg(lockFile));

        return true;
    }

    QString line(file.readLine(100));

    pid_t pid = line.toInt(&bOK);

    if (!bOK)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Got bad PID '%1' from lock file").arg(pid));
        return true;
    }

    LOG(VB_GENERAL, LOG_NOTICE,
        QString("Checking if PID %1 is still running").arg(pid));

#ifdef _WIN32
    HANDLE handy = OpenProcess(SYNCHRONIZE|PROCESS_TERMINATE, TRUE, pid);
    return TerminateProcess(handy,0) == 0;
#else
    if (kill(pid, 0) == -1)
    {
        if (errno == ESRCH)
            return false;
    }
#endif

    return true;
}

// return true if a lock file is found and the owning process is still running
static bool checkLockFile(const QString &lockFile)
{
    QFile file(lockFile);

    //is a job already running?
    if (file.exists())
    {
        // Is the process that created the lock still alive?
        if (!checkProcess(lockFile))
        {
            showWarningDialog(QCoreApplication::translate("(MythArchiveMain)",
                "Found a lock file but the owning process isn't running!\n"
                "Removing stale lock file."));
            if (!file.remove())
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("Failed to remove stale lock file - %1")
                        .arg(lockFile));
            }
        }
        else
        {
            return true;
        }
    }

    return false;
}

static void runCreateDVD(void)
{
    QString tempDir = getTempDirectory(true);
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    if (tempDir == "")
        return;

    QString logDir = tempDir + "logs";

    checkTempDirectory();

    if (checkLockFile(logDir + "/mythburn.lck"))
    {
        // a job is already running so just show the log viewer
        showLogViewer();
        return;
    }

    // show the select destination dialog
    auto *dest = new SelectDestination(mainStack, false, "SelectDestination");

    if (dest->Create())
        mainStack->AddScreen(dest);
}

static void runCreateArchive(void)
{
    QString tempDir = getTempDirectory(true);
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    if (tempDir == "")
        return;

    QString logDir = tempDir + "logs";

    checkTempDirectory();

    if (checkLockFile(logDir + "/mythburn.lck"))
    {
        // a job is already running so just show the log viewer
        showLogViewer();
        return;
    }

    // show the select destination dialog
    auto *dest = new SelectDestination(mainStack, true, "SelectDestination");

    if (dest->Create())
        mainStack->AddScreen(dest);
}

static void runEncodeVideo(void)
{

}

static void runImportVideo(void)
{
    QString tempDir = getTempDirectory(true);

    if (tempDir == "")
        return;

    QString logDir = tempDir + "logs";

    checkTempDirectory();

    if (checkLockFile(logDir + "/mythburn.lck"))
    {
        // a job is already running so just show the log viewer
        showLogViewer();
        return;
    }

    // show the find archive screen
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *selector = new ArchiveFileSelector(mainStack);

    if (selector->Create())
        mainStack->AddScreen(selector);
}

static void runShowLog(void)
{
    showLogViewer();
}

static void runTestDVD(void)
{
    if (!gCoreContext->GetSetting("MythArchiveLastRunType").startsWith("DVD"))
    {
        showWarningDialog(QCoreApplication::translate("(MythArchiveMain)",
            "Last run did not create a playable DVD."));
        return;
    }

    if (!gCoreContext->GetSetting("MythArchiveLastRunStatus").startsWith("Success"))
    {
        showWarningDialog(QCoreApplication::translate("(MythArchiveMain)",
                                          "Last run failed to create a DVD."));
        return;
    }

    QString tempDir = getTempDirectory(true);

    if (tempDir == "")
        return;

    QString filename = tempDir + "work/dvd";
    QString command = gCoreContext->GetSetting("MythArchiveDVDPlayerCmd", "");

    if ((command.indexOf("internal", 0, Qt::CaseInsensitive) > -1) ||
         (command.length() < 1))
    {
        filename = QString("dvd:/") + filename;
        command = "Internal";
        GetMythMainWindow()->HandleMedia(command, filename);
        return;
    }

    if (command.contains("%f"))
        command = command.replace("%f", filename);
    myth_system(command);
}

static void runBurnDVD(void)
{
    auto *menu = new BurnMenu();
    menu->start();
}

// these point to the the mainmenu callback if found
static void (*m_callback)(void *, QString &) = nullptr;
static void *m_callbackdata = nullptr;

static void ArchiveCallback([[maybe_unused]] void *data, QString &selection)
{
    QString sel = selection.toLower();

    if (sel == "archive_create_dvd")
        runCreateDVD();
    else if (sel == "archive_create_archive")
        runCreateArchive();
    else if (sel == "archive_encode_video")
        runEncodeVideo();
    else if (sel == "archive_import_video")
        runImportVideo();
    else if (sel == "archive_last_log")
        runShowLog();
    else if (sel == "archive_test_dvd")
        runTestDVD();
    else if (sel == "archive_burn_dvd")
        runBurnDVD();
    else
    {
        // if we have found the mainmenu callback
        // pass the selection on to it
        if (m_callback && m_callbackdata)
            m_callback(m_callbackdata, selection);
    }
}

static int runMenu(const QString& which_menu)
{
    // find the 'mainmenu' MythThemedMenu so we can use the callback from it
    MythThemedMenu *mainMenu = nullptr;
    QObject *parentObject = GetMythMainWindow()->GetMainStack()->GetTopScreen();

    while (parentObject)
    {
        mainMenu = qobject_cast<MythThemedMenu *>(parentObject);

        if (mainMenu && mainMenu->objectName() == "mainmenu")
            break;

        parentObject = parentObject->parent();
    }

    QString themedir = GetMythUI()->GetThemeDir();
    auto *diag = new MythThemedMenu(themedir, which_menu,
                                    GetMythMainWindow()->GetMainStack(),
                                    "archive menu");

    // save the callback from the main menu
    if (mainMenu)
        mainMenu->getCallback(&m_callback, &m_callbackdata);
    else
    {
        m_callback = nullptr;
        m_callbackdata = nullptr;
    }

    diag->setCallback(ArchiveCallback, nullptr);
    diag->setKillable();

    if (diag->foundTheme())
    {
        GetMythMainWindow()->GetMainStack()->AddScreen(diag);
        return 0;
    }

    LOG(VB_GENERAL, LOG_ERR, QString("Couldn't find menu %1 or theme %2")
        .arg(which_menu, themedir));
    delete diag;
    return -1;
}

static void initKeys(void)
{
    REG_KEY("Archive", "TOGGLECUT", QT_TRANSLATE_NOOP("MythControls",
        "Toggle use cut list state for selected program"), "C");

    REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Create DVD"),
        "", "", runCreateDVD);
    REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Create Archive"),
        "", "", runCreateArchive);
    REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Import Archive"),
        "", "", runImportVideo);
    REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "View Archive Log"),
        "", "", runShowLog);
    REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Play Created DVD"),
        "", "", runTestDVD);
    REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Burn DVD"),
        "", "", runBurnDVD);
}

int mythplugin_init(const char *libversion)
{
    if (!MythCoreContext::TestPluginVersion("mytharchive", libversion,
                                            MYTH_BINARY_VERSION))
    {
        LOG(VB_GENERAL, LOG_ERR, "Test Popup Version Failed");
        return -1;
    }

    gCoreContext->ActivateSettingsCache(false);
    if (!UpgradeArchiveDatabaseSchema())
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Couldn't upgrade database to new schema, exiting.");
        return -1;
    }
    gCoreContext->ActivateSettingsCache(true);

    ArchiveSettings settings;
    settings.Load();
    settings.Save();

    initKeys();

    return 0;
}

int mythplugin_run(void)
{
    return runMenu("archivemenu.xml");
}

int mythplugin_config(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *ssd = new StandardSettingDialog(mainStack, "archivesettings",
                                          new ArchiveSettings());

    if (ssd->Create())
    {
        mainStack->AddScreen(ssd);
    }
    else
    {
        delete ssd;
    }

    return 0;
}
