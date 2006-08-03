/*
	main.cpp

	MythArchive - mythtv plugin
	
	Starting point for the MythArchive module
*/
#include "config.h"

#include <iostream>
using namespace std;

// Qt
#include <qapplication.h>
#include <qdir.h>
#include <qtimer.h>

// mythtv
#include <mythtv/mythcontext.h>
#include <mythtv/mythplugin.h>
#include <mythtv/dialogbox.h>
#include <mythtv/util.h>
#include <mythtv/libmythui/myththemedmenu.h>

// mytharchive
#include "archivesettings.h"
#include "logviewer.h"
#include "fileselector.h"
#include "recordingselector.h"
#include "videoselector.h"
#include "dbcheck.h"

#ifdef CREATE_DVD 
    #include "mythburnwizard.h"
#endif

#ifdef CREATE_NATIVE
    #include "exportnativewizard.h"
    #include "importnativewizard.h"
#endif


void runCreateDVD(void)
{
#ifdef CREATE_DVD 
    int res;

    QString commandline;
    QString tempDir = getTempDirectory();

    if (tempDir == "")
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), 
                                  QObject::tr("Myth Archive"),
                                  QObject::tr("Cannot find the MythArchive work directory.\n" 
                                          "Have you set the correct path in the settings?"));  
        return;
    }

    QString logDir = tempDir + "logs";
    QString configDir = tempDir + "config";
    QString workDir = tempDir + "work";

    checkTempDirectory();

    QFile file(logDir + "/mythburn.lck");

    //Are we already building a recording?
    if ( file.exists() )
    {
        // Yes so we just show the log viewer
        LogViewer dialog(gContext->GetMainWindow(), "logviewer");
        dialog.setFilename(logDir + "/progress.log");
        dialog.exec();
        return;
    }

    // show the mythburn wizard
    MythburnWizard *burnWiz;

    burnWiz = new MythburnWizard(gContext->GetMainWindow(),
                             "mythburn_wizard", "mythburn-");
    qApp->unlock();
    res = burnWiz->exec();
    qApp->lock();
    qApp->processEvents();
    delete burnWiz;

    if (res == 0)
        return;

    // now show the log viewer
    LogViewer dialog(gContext->GetMainWindow(), "logviewer");
    dialog.setFilename(logDir + "/progress.log");
    dialog.exec();
#else
    cout << "DVD creation is not compiled in!!" << endl;
#endif
}

void runCreateArchive(void)
{
#ifdef CREATE_NATIVE
    int res;

    QString commandline;
    QString tempDir = getTempDirectory();

    if (tempDir == "")
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), 
                                  QObject::tr("Myth Archive"),
                                  QObject::tr("Cannot find the MythArchive work directory.\n" 
                                          "Have you set the correct path in the settings?"));  
        return;
    }

    QString logDir = tempDir + "logs";
    QString configDir = tempDir + "config";
    QString workDir = tempDir + "work";

    checkTempDirectory();

    QFile file(logDir + "/mythburn.lck");

    //Are we already building a recording?
    if ( file.exists() )
    {
        // Yes so we just show the log viewer
        LogViewer dialog(gContext->GetMainWindow(), "logviewer");
        dialog.setFilename(logDir + "/progress.log");
        dialog.exec();
        return;
    }

    // show the export native wizard
    ExportNativeWizard *nativeWiz;

    nativeWiz = new ExportNativeWizard(gContext->GetMainWindow(),
                                 "exportnative_wizard", "mythnative-");
    qApp->unlock();
    res = nativeWiz->exec();
    qApp->lock();
    qApp->processEvents();
    delete nativeWiz;

    if (res == 0)
        return;

    // now show the log viewer
    LogViewer dialog(gContext->GetMainWindow(), "logviewer");
    dialog.setFilename(logDir + "/progress.log");
    dialog.exec();
#else
    cout << "Native archive creation is not compiled in!!" << endl;
#endif
}

void runEncodeVideo(void)
{

}

void runImportVideo(void)
{
#ifdef CREATE_NATIVE
    QString tempDir = getTempDirectory();

    if (tempDir == "")
    {
        MythPopupBox::showOkPopup(gContext->GetMainWindow(), 
                                  QObject::tr("Myth Archive"),
                                  QObject::tr("Cannot find the MythArchive work directory.\n" 
                                          "Have you set the correct path in the settings?"));  
        return;
    }

    QString logDir = tempDir + "logs";
    QString configDir = tempDir + "config";
    QString workDir = tempDir + "work";

    checkTempDirectory();

    QFile file(logDir + "/mythburn.lck");

    //Are we already building a recording?
    if ( file.exists() )
    {
        // Yes so we just show the log viewer
        LogViewer dialog(gContext->GetMainWindow(), "logviewer");
        dialog.setFilename(logDir + "/progress.log");
        dialog.exec();
        return;
    }

    QString filter = "*.xml";

    ImportNativeWizard wiz("/", filter, gContext->GetMainWindow(),
                          "import_native_wizard", "mythnative-", "import native wizard");
    qApp->unlock();
    int res = wiz.exec();
    qApp->lock();

    if (res == 0)
        return;

    // now show the log viewer
    LogViewer dialog(gContext->GetMainWindow(), "logviewer");
    dialog.setFilename(logDir + "/progress.log");
    dialog.exec();
#else
    cout << "Native archive creation is not compiled in!!" << endl;
#endif
}

void runRecordingSelector(void)
{
    RecordingSelector selector(gContext->GetMainWindow(),
                          "recording_selector", "mytharchive-", "recording selector");
    qApp->unlock();
    selector.exec();
    qApp->lock();
}

void runVideoSelector(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT title FROM videometadata");
    query.exec();
    if (query.isActive() && query.numRowsAffected())
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
    qApp->unlock();
    selector.exec();
    qApp->lock();
}

void runFileSelector(void)
{
    QString filter = gContext->GetSetting("MythArchiveFileFilter",
                                          "*.mpg *.mpeg *.mov *.avi *.nuv");

    FileSelector selector(FSTYPE_FILELIST, "/", filter, gContext->GetMainWindow(),
                       "file_selector", "mytharchive-", "file selector");
    qApp->unlock();
    selector.exec();
    qApp->lock();
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
    QString themedir = gContext->GetThemeDir();
    MythThemedMenu *diag = new MythThemedMenu(themedir.ascii(), which_menu, 
                                      GetMythMainWindow()->GetMainStack(),
                                      "select menu");
    diag->setCallback(SelectorCallback, NULL);
    diag->setKillable();

    if (diag->foundTheme())
    {
        GetMythMainWindow()->GetMainStack()->AddScreen(diag);
    }
    else
    {
        cerr << "Couldn't find theme " << themedir << endl;
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
    QString themedir = gContext->GetThemeDir();
    MythThemedMenu *diag = new MythThemedMenu(themedir.ascii(), which_menu, 
                                              GetMythMainWindow()->GetMainStack(),
                                              "format menu");
    diag->setCallback(FormatCallback, NULL);
    diag->setKillable();

    if (diag->foundTheme())
    {
        GetMythMainWindow()->GetMainStack()->AddScreen(diag);
    }
    else
    {
        cerr << "Couldn't find theme " << themedir << endl;
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
}

void runMenu(QString which_menu)
{
    QString themedir = gContext->GetThemeDir();

    MythThemedMenu *diag = new MythThemedMenu(themedir.ascii(), which_menu, 
                                      GetMythMainWindow()->GetMainStack(),
                                      "archive menu");

    diag->setCallback(ArchiveCallback, NULL);
    diag->setKillable();

    if (diag->foundTheme())
    {
        GetMythMainWindow()->GetMainStack()->AddScreen(diag);
    }
    else
    {
        cerr << "Couldn't find theme " << themedir << endl;
    }
}


extern "C" {
int mythplugin_init(const char *libversion);
int mythplugin_run(void);
int mythplugin_config(void);
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
        cerr << "Test Popup Version Failed " << endl;
        return -1;
    }

    gContext->ActivateSettingsCache(false);
    UpgradeArchiveDatabaseSchema();
    gContext->ActivateSettingsCache(false);

    ArchiveSettings mpSettings;
    mpSettings.load();
    mpSettings.save();

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
