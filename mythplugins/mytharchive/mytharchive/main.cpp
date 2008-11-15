/*
	main.cpp

	MythArchive - mythtv plugin
	
	Starting point for the MythArchive module
*/
#include "config.h"

#include <iostream>
#include <cstdlib>
#include <signal.h>

using namespace std;

// Qt
#include <QApplication>
#include <QDir>
#include <QTimer>

// mythtv
#include <mythtv/mythcontext.h>
#include <mythtv/mythversion.h>
#include <mythtv/mythplugin.h>
#include <mythtv/dialogbox.h>
#include <mythtv/util.h>
#include <mythtv/libmythui/myththemedmenu.h>
#include <mythtv/mythpluginapi.h>
#include <mythtv/libmythui/mythuihelper.h>

// mytharchive
#include "archivesettings.h"
#include "logviewer.h"
#include "fileselector.h"
#include "recordingselector.h"
#include "videoselector.h"
#include "dbcheck.h"
#include "archiveutil.h"
#include "selectdestination.h"

#ifdef CREATE_DVD
    #include "mythburnwizard.h"
#endif

#ifdef CREATE_NATIVE
    #include "exportnative.h"
    #include "importnativewizard.h"
#endif

// return true if the process belonging to the lock file is still running
bool checkProcess(const QString &lockFile)
{
    // read the PID from the lock file
    QFile file(lockFile);

    bool bOK = file.open(QIODevice::ReadOnly);

    if (!bOK)
    {
        VERBOSE(VB_GENERAL, QString("Unable to open file %1").arg(lockFile));

        return true;
    }

    QString line(file.readLine(100));

    pid_t pid = line.toInt(&bOK);

    if (!bOK)
    {
        VERBOSE(VB_GENERAL, QString("Got bad PID '%1' from lock file").arg(pid));
        return true;
    }

    VERBOSE(VB_GENERAL, QString("Checking if PID %1 is still running").arg(pid));

    if (kill(pid, 0) == -1)
    {
        if (errno == ESRCH)
            return false;
    }

    return true;
}

// return true if a lock file is found and the owning process is still running
bool checkLockFile(const QString &lockFile)
{
    QFile file(lockFile);

    //is a job already running?
    if (file.exists())
    {
        // Is the process that created the lock still alive?
        if (!checkProcess(lockFile))
        {
            showWarningDialog(QObject::tr("Found a lock file but the owning process isn't running!\n"
                                          "Removing stale lock file."));
            if (!file.remove())
                VERBOSE(VB_IMPORTANT, QString("Failed to remove stale lock file - %1")
                        .arg(lockFile));
        }
        else
        {
            return true;
        }
    }

    return false;
}

void runCreateDVD(void)
{
#ifdef CREATE_DVD
    QString commandline;
    QString tempDir = getTempDirectory(true);

    if (tempDir == "")
        return;

    QString logDir = tempDir + "logs";
    QString configDir = tempDir + "config";
    QString workDir = tempDir + "work";

    checkTempDirectory();

    if (checkLockFile(logDir + "/mythburn.lck"))
    {
        // a job is already running so just show the log viewer
        LogViewer dialog(gContext->GetMainWindow(), "logviewer");
        dialog.setFilenames(logDir + "/progress.log", logDir + "/mythburn.log");
        dialog.exec();
        return;
    }

    // show the mythburn wizard
    MythburnWizard *burnWiz;

    burnWiz = new MythburnWizard(gContext->GetMainWindow(),
                             "mythburn_wizard", "mythburn-");
    DialogCode res = burnWiz->exec();
    qApp->processEvents();
    delete burnWiz;

    if (kDialogCodeRejected == res)
        return;

    // now show the log viewer
    LogViewer dialog(gContext->GetMainWindow(), "logviewer");
    dialog.setFilenames(logDir + "/progress.log", logDir + "/mythburn.log");
    dialog.exec();
#else
    VERBOSE(VB_IMPORTANT, "DVD creation is not compiled in!!");
#endif
}

void runCreateArchive(void)
{
#ifdef CREATE_NATIVE
    QString commandline;
    QString tempDir = getTempDirectory(true);

    if (tempDir == "")
        return;

    QString logDir = tempDir + "logs";
    QString configDir = tempDir + "config";
    QString workDir = tempDir + "work";

    checkTempDirectory();

    if (checkLockFile(logDir + "/mythburn.lck"))
    {
        // a job is already running so just show the log viewer
        LogViewer dialog(gContext->GetMainWindow(), "logviewer");
        dialog.setFilenames(logDir + "/progress.log", logDir + "/mythburn.log");
        dialog.exec();
        return;
    }

    // show the select destination dialog
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    SelectDestination *dest = new SelectDestination(mainStack, "SelectDestination");

    if (dest->Create())
        mainStack->AddScreen(dest);

    return;

    // now show the log viewer
    LogViewer dialog(gContext->GetMainWindow(), "logviewer");
    dialog.setFilenames(logDir + "/progress.log", logDir + "/mythburn.log");
    dialog.exec();
#else
    VERBOSE(VB_IMPORTANT, "Native archive creation is not compiled in!!");
#endif
}

void runEncodeVideo(void)
{

}

void runImportVideo(void)
{
#ifdef CREATE_NATIVE
    QString tempDir = getTempDirectory(true);

    if (tempDir == "")
        return;

    QString logDir = tempDir + "logs";
    QString configDir = tempDir + "config";
    QString workDir = tempDir + "work";

    checkTempDirectory();

    if (checkLockFile(logDir + "/mythburn.lck"))
    {
        // a job is already running so just show the log viewer
        LogViewer dialog(gContext->GetMainWindow(), "logviewer");
        dialog.setFilenames(logDir + "/progress.log", logDir + "/mythburn.log");
        dialog.exec();
        return;
    }

    QString filter = "*.xml";

    ImportNativeWizard wiz("/", filter, gContext->GetMainWindow(),
                          "import_native_wizard", "mythnative-", "import native wizard");
    DialogCode res = wiz.exec();

    if (kDialogCodeRejected == res)
        return;

    // now show the log viewer
    LogViewer dialog(gContext->GetMainWindow(), "logviewer");
    dialog.setFilenames(logDir + "/progress.log", logDir + "/mythburn.log");
    dialog.exec();
#else
    VERBOSE(VB_IMPORTANT, "Native archive creation is not compiled in!!");
#endif
}

void runShowLog(void)
{
    QString tempDir = getTempDirectory(true);

    if (tempDir == "")
        return;

    QString logDir = tempDir + "logs";

    // do any logs exist?
    if (QFile::exists(logDir + "/progress.log") || QFile::exists(logDir + "/mythburn.log"))
    {
        LogViewer dialog(gContext->GetMainWindow(), "logviewer");
        dialog.setFilenames(logDir + "/progress.log", logDir + "/mythburn.log");
        dialog.exec();
    }
    else
        showWarningDialog(QObject::tr("Cannot find any logs to show!"));
}

void runTestDVD(void)
{
    if (!gContext->GetSetting("MythArchiveLastRunType").startsWith("DVD"))
    {
        showWarningDialog(QObject::tr("Last run did not create a playable DVD."));
        return;
    }

    if (!gContext->GetSetting("MythArchiveLastRunStatus").startsWith("Success"))
    {
        showWarningDialog(QObject::tr("Last run failed to create a DVD."));
        return;
    }

    QString tempDir = getTempDirectory(true);

    if (tempDir == "")
        return;

    QString filename = tempDir + "work/dvd/";
    QString command = gContext->GetSetting("MythArchiveDVDPlayerCmd", "");

    if ((command.find("internal", 0, false) > -1) || (command.length() < 1))
    {
        filename = QString("dvd:/") + filename;
        command = "Internal";
        gContext->GetMainWindow()->HandleMedia(command, filename);
        return;
    }
    else
    {
        if (command.contains("%f"))
            command = command.replace(QRegExp("%f"), filename);
        myth_system(command);
    }
}

void runBurnDVD(void)
{
    if (!gContext->GetSetting("MythArchiveLastRunStatus").startsWith("Success"))
    {
        showWarningDialog(QObject::tr("Cannot burn a DVD.\nThe last run failed to create a DVD."));
        return;
    }

    // ask the user what type of disk to burn to
    DialogBox *dialog = new DialogBox(gContext->GetMainWindow(),
            QObject::tr("\nPlace a blank DVD in the drive and select an option below."));

    dialog->AddButton(QObject::tr("Burn DVD"));
    dialog->AddButton(QObject::tr("Burn DVD Rewritable"));
    dialog->AddButton(QObject::tr("Burn DVD Rewritable (Force Erase)"));
    dialog->AddButton(QObject::tr("Cancel"));

    DialogCode res = dialog->exec();
    dialog->deleteLater();

    // cancel pressed or escape hit?
    if ((kDialogCodeButton3 == res) || (kDialogCodeRejected == res))
        return;

    int fmt = MythDialog::CalcItemIndex(res);
    if ((fmt < 0) || (fmt > 2))
        return;

    QString tempDir = getTempDirectory(true);

    if (tempDir == "")
        return;

    QString logDir = tempDir + "logs";
    QString configDir = tempDir + "config";
    QString commandline;

    // remove existing progress.log if present
    if (QFile::exists(logDir + "/progress.log"))
        QFile::remove(logDir + "/progress.log");

    // remove cancel flag file if present
    if (QFile::exists(logDir + "/mythburncancel.lck"))
        QFile::remove(logDir + "/mythburncancel.lck");

    QString sArchiveFormat = QString::number(fmt);
    QString sEraseDVDRW = (kDialogCodeButton2 == res) ? "1" : "0";
    QString sNativeFormat = (gContext->GetSetting("MythArchiveLastRunType").startsWith("Native") ? "1" : "0");

    commandline = "mytharchivehelper -b " + sArchiveFormat + " " + sEraseDVDRW  + " " + sNativeFormat;
    commandline += " > "  + logDir + "/progress.log 2>&1 &";
    int state = system(commandline);

    if (state != 0)
    {
        showWarningDialog(QObject::tr("It was not possible to run mytharchivehelper to burn the DVD."));
        return;
    }

    // now show the log viewer
    LogViewer logViewer(gContext->GetMainWindow(), "logviewer");
    logViewer.setFilenames(logDir + "/progress.log", logDir + "/mythburn.log");
    logViewer.exec();
}

void runRecordingSelector(void)
{
    RecordingSelector selector(gContext->GetMainWindow(),
                          "recording_selector", "mytharchive-", "recording selector");
    selector.exec();
}

void runVideoSelector(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT title FROM videometadata");
    query.exec();
    if (query.isActive() && query.size())
    {
    }
    else
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), QObject::tr("Video Selector"),
                                  QObject::tr("You don't have any videos!"));
        return;
    }

    VideoSelector selector(gContext->GetMainWindow(),
                          "video_selector", "mytharchive-", "video selector");
    selector.exec();
}

void runFileSelector(void)
{
    QString filter = gContext->GetSetting("MythArchiveFileFilter",
                                          "*.mpg *.mpeg *.mov *.avi *.nuv");

    FileSelector selector(FSTYPE_FILELIST, "/", filter, gContext->GetMainWindow(),
                       "file_selector", "mytharchive-", "file selector");
    selector.exec();
}

void SelectorCallback(void *data, QString &selection)
{
    (void) data;

    QString sel = selection.lower();

    if (sel == "archive_select_recordings")
        runRecordingSelector();
    else if (sel == "archive_select_videos")
        runVideoSelector();
    else if (sel == "archive_select_files")
        runFileSelector();
}

void runSelectMenu(QString which_menu)
{
    QString themedir = GetMythUI()->GetThemeDir();
    MythThemedMenu *diag = new MythThemedMenu(
        themedir, which_menu, GetMythMainWindow()->GetMainStack(),
        "select menu");

    diag->setCallback(SelectorCallback, NULL);
    diag->setKillable();

    if (diag->foundTheme())
    {
        GetMythMainWindow()->GetMainStack()->AddScreen(diag);
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("Couldn't find menu %1 or theme %2")
                              .arg(which_menu).arg(themedir));
        delete diag;
    }
}

void FormatCallback(void *data, QString &selection)
{
    (void) data;

    QString sel = selection.lower();
    if (sel == "archive_create_dvd")
        runCreateDVD();
    else if (sel == "archive_create_archive")
        runCreateArchive();
    else if (sel == "archive_encode_video")
        runEncodeVideo();
}

void runFormatMenu(QString which_menu)
{
    QString themedir = GetMythUI()->GetThemeDir();
    MythThemedMenu *diag = new MythThemedMenu(
        themedir, which_menu, GetMythMainWindow()->GetMainStack(),
        "format menu");

    diag->setCallback(FormatCallback, NULL);
    diag->setKillable();

    if (diag->foundTheme())
    {
        GetMythMainWindow()->GetMainStack()->AddScreen(diag);
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("Couldn't find menu %1 or theme %2")
                              .arg(which_menu).arg(themedir));
        delete diag;
    }
}

void ArchiveCallback(void *data, QString &selection)
{
    (void) data;

    QString sel = selection.lower();

    if (sel == "archive_finder")
        runSelectMenu("archiveselect.xml");
    else if (sel == "archive_export_video")
        runFormatMenu("archiveformat.xml");
    else if (sel == "archive_import_video")
        runImportVideo();
    else if (sel == "archive_last_log")
        runShowLog();
    else if (sel == "archive_test_dvd")
        runTestDVD();
    else if (sel == "archive_burn_dvd")
        runBurnDVD();
}

void runMenu(QString which_menu)
{
    QString themedir = GetMythUI()->GetThemeDir();
    MythThemedMenu *diag = new MythThemedMenu(
        themedir, which_menu, GetMythMainWindow()->GetMainStack(),
        "archive menu");

    diag->setCallback(ArchiveCallback, NULL);
    diag->setKillable();

    if (diag->foundTheme())
    {
        GetMythMainWindow()->GetMainStack()->AddScreen(diag);
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("Couldn't find menu %1 or theme %2")
                              .arg(which_menu).arg(themedir));
        delete diag;
    }
}

void initKeys(void)
{
    REG_KEY("Archive", "TOGGLECUT", "Toggle use cut list state for selected program", "C");
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mytharchive", libversion,
                                    MYTH_BINARY_VERSION))
    {
        VERBOSE(VB_IMPORTANT, "Test Popup Version Failed ");
        return -1;
    }

    gContext->ActivateSettingsCache(false);
    if (!UpgradeArchiveDatabaseSchema())
    {
        VERBOSE(VB_IMPORTANT,
                "Couldn't upgrade database to new schema, exiting.");
        return -1;
    }
    gContext->ActivateSettingsCache(false);

    ArchiveSettings settings;
    settings.Load();
    settings.Save();

    initKeys();

    return 0;
}

int mythplugin_run(void)
{
    runMenu("archivemenu.xml");

    return 0;
}

int mythplugin_config(void)
{
    ArchiveSettings settings;
    settings.exec();

    return 0;
}
