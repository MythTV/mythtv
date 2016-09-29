#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <cerrno>
#include <stdlib.h>

#include <iostream>
using namespace std;

#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QKeyEvent>
#include <QEvent>
#include <QDir>
#include <QTextCodec>
#include <QWidget>
#include <QApplication>
#include <QTimer>
#ifdef Q_OS_MAC
#include <QProcessEnvironment>
#endif

#include "previewgeneratorqueue.h"
#include "referencecounter.h"
#include "mythmiscutil.h"
#include "mythconfig.h"
#include "mythcdrom.h"
#include "mythsystemlegacy.h"
#include "tv.h"
#include "proglist.h"
#include "progfind.h"
#include "scheduleeditor.h"
#include "manualschedule.h"
#include "playbackbox.h"
#include "themechooser.h"
#include "setupwizard_general.h"
#include "customedit.h"
#include "viewscheduled.h"
#include "programrecpriority.h"
#include "channelrecpriority.h"
#include "custompriority.h"
#include "audiooutput.h"
#include "globalsettings.h"
#include "audiogeneralsettings.h"
#include "grabbersettings.h"
#include "playgroup.h"
#include "networkcontrol.h"
#include "scheduledrecording.h"
#include "mythsystemevent.h"
#include "hardwareprofile.h"
#include "signalhandling.h"
#include "loggingserver.h"

#include "compat.h"  // For SIG* on MinGW
#include "exitcodes.h"
#include "exitprompt.h"
#include "programinfo.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "guidegrid.h"
#include "mythplugin.h"
#include "remoteutil.h"
#include "dbcheck.h"
#include "mythmediamonitor.h"
#include "statusbox.h"
#include "idlescreen.h"
#include "lcddevice.h"
#include "langsettings.h"
#include "mythtranslation.h"
#include "commandlineparser.h"
#include "channelgroupsettings.h"
#include "tvremoteutil.h"

#include "myththemedmenu.h"
#include "mediarenderer.h"
#include "mythmainwindow.h"
#include "mythcontrols.h"
#include "mythuihelper.h"
#include "mythdirs.h"
#include "mythdb.h"
#include "backendconnectionmanager.h"
#include "themechooser.h"
#include "mythversion.h"
#include "taskqueue.h"
#include "cleanupguard.h"

// Video
#include "cleanup.h"
#include "globals.h"
#include "videodlg.h"
#include "videoglobalsettings.h"
#include "videofileassoc.h"
#include "videoplayersettings.h"
#include "videometadatasettings.h"
#include "videolist.h"

// Gallery
#include "gallerythumbview.h"

// DVD & Bluray
#include "DVD/dvdringbuffer.h"
#include "Bluray/bdringbuffer.h"

// AirPlay
#ifdef USING_AIRPLAY
#include "AirPlay/mythraopdevice.h"
#include "AirPlay/mythairplayserver.h"
#endif

#ifdef USING_LIBDNS_SD
#include <QScopedPointer>
#include "bonjourregister.h"
#endif

static ExitPrompter   *exitPopup = NULL;
static MythThemedMenu *menu;

static QString         logfile;
static MediaRenderer  *g_pUPnp   = NULL;
static MythPluginManager *pmanager = NULL;

static void handleExit(bool prompt);
static void resetAllKeys(void);
void handleSIGUSR1(void);
void handleSIGUSR2(void);

#if CONFIG_DARWIN
static bool gLoaded = false;
#endif

static const QString _Location = qApp->translate("(Common)",
                                                 "MythFrontend");

namespace
{
    class RunSettingsCompletion : public QObject
    {
        Q_OBJECT

      public:
        static void Create(bool check)
        {
            new RunSettingsCompletion(check);
        }

      private:
        explicit RunSettingsCompletion(bool check)
        {
            if (check)
            {
                connect(&m_plcc,
                        SIGNAL(SigResultReady(bool, ParentalLevel::Level)),
                        SLOT(OnPasswordResultReady(bool,
                                        ParentalLevel::Level)));
                m_plcc.Check(ParentalLevel::plMedium, ParentalLevel::plHigh);
            }
            else
            {
                OnPasswordResultReady(true, ParentalLevel::plHigh);
            }
        }

        ~RunSettingsCompletion() {}

      private slots:
        void OnPasswordResultReady(bool passwordValid,
                ParentalLevel::Level newLevel)
        {
            (void) newLevel;

            if (passwordValid)
            {
                VideoGeneralSettings settings;
                settings.exec();
            }
            else
            {
                LOG(VB_GENERAL, LOG_WARNING,
                        "Aggressive Parental Controls Warning: "
                        "invalid password. An attempt to enter a "
                        "MythVideo settings screen was prevented.");
            }

            deleteLater();
        }

      public:
        ParentalLevelChangeChecker m_plcc;
    };

    class BookmarkDialog : MythScreenType
    {
        Q_DECLARE_TR_FUNCTIONS(BookmarkDialog)

      public:
        BookmarkDialog(ProgramInfo *pginfo, MythScreenStack *parent) :
                MythScreenType(parent, "bookmarkdialog"),
                pgi(pginfo)
        {
        }

        bool Create()
        {
            QString msg = tr("DVD/Video contains a bookmark");
            QString btn0msg = tr("Play from bookmark");
            QString btn1msg = tr("Play from beginning");

            MythDialogBox *popup = new MythDialogBox(msg, GetScreenStack(), "bookmarkdialog");
            if (!popup->Create())
            {
                delete popup;
                return false;
            }

            GetScreenStack()->AddScreen(popup);

            popup->SetReturnEvent(this, "bookmarkdialog");
            popup->AddButton(btn0msg);
            popup->AddButton(btn1msg);
            return true;
        }

        void customEvent(QEvent *event)
        {
            if (event->type() == DialogCompletionEvent::kEventType)
            {
                DialogCompletionEvent *dce = (DialogCompletionEvent*)(event);
                int buttonnum = dce->GetResult();

                if (dce->GetId() == "bookmarkdialog")
                {
                    uint flags = kStartTVNoFlags;
                    if (buttonnum == 1)
                    {
                        flags |= kStartTVIgnoreBookmark;
                    }
                    else if (buttonnum != 0)
                    {
                        delete pgi;
                        return;
                    }

                    TV::StartTV(pgi, flags);

                    delete pgi;
                }
            }
        }

      private:
        ProgramInfo* pgi;
    };

    void cleanup()
    {
#ifdef USING_AIRPLAY
        MythRAOPDevice::Cleanup();
        MythAirplayServer::Cleanup();
#endif

        delete exitPopup;
        exitPopup = NULL;

        AudioOutput::Cleanup();

        if (g_pUPnp)
        {
            // This takes a few seconds, so inform the user:
            LOG(VB_GENERAL, LOG_INFO, "Shutting down UPnP client...");
            delete g_pUPnp;
            g_pUPnp = NULL;
        }

        if (pmanager)
        {
            delete pmanager;
            pmanager = NULL;
        }

        DestroyMythMainWindow();

        delete gContext;
        gContext = NULL;

        ReferenceCounter::PrintDebug();

        delete qApp;

        SignalHandler::Done();
    }
}

static void startAppearWiz(void)
{
    int curX = gCoreContext->GetNumSetting("GuiOffsetX", 0);
    int curY = gCoreContext->GetNumSetting("GuiOffsetY", 0);
    int curW = gCoreContext->GetNumSetting("GuiWidth", 0);
    int curH = gCoreContext->GetNumSetting("GuiHeight", 0);

    bool isWindowed =
            (gCoreContext->GetNumSetting("RunFrontendInWindow", 0) == 1);

    bool reload = false;

    if (isWindowed)
        ShowOkPopup(qApp->translate("(MythFrontendMain)",
                    "The ScreenSetupWizard cannot be used while "
                    "mythfrontend is operating in windowed mode."));
    else
    {
        MythSystemLegacy *wizard = new MythSystemLegacy(
            GetAppBinDir() + "mythscreenwizard",
            QStringList(),
            kMSDisableUDPListener | kMSPropagateLogs);
        wizard->Run();

        if (!wizard->Wait())
        {
            // no reported errors, check for changed geometry parameters
            gCoreContext->ClearSettingsCache("GuiOffsetX");
            gCoreContext->ClearSettingsCache("GuiOffsetY");
            gCoreContext->ClearSettingsCache("GuiWidth");
            gCoreContext->ClearSettingsCache("GuiHeight");

            if ((curX != gCoreContext->GetNumSetting("GuiOffsetX", 0)) ||
                (curY != gCoreContext->GetNumSetting("GuiOffsetY", 0)) ||
                (curW != gCoreContext->GetNumSetting("GuiWidth", 0)) ||
                (curH != gCoreContext->GetNumSetting("GuiHeight", 0)))
                    reload = true;
        }

        delete wizard;
        wizard = NULL;
    }

    if (reload)
        GetMythMainWindow()->JumpTo("Reload Theme");
}

static void startKeysSetup()
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    MythControls *mythcontrols = new MythControls(mainStack, "mythcontrols");

    if (mythcontrols->Create())
        mainStack->AddScreen(mythcontrols);
    else
        delete mythcontrols;
}

static void startGuide(void)
{
    uint chanid = 0;
    QString channum = gCoreContext->GetSetting("DefaultTVChannel");
    QDateTime startTime;
    GuideGrid::RunProgramGuide(chanid, channum, startTime, NULL, false, true, -2);
}

static void startFinder(void)
{
    RunProgramFinder();
}

static void startSearchTitle(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plTitleSearch, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

static void startSearchKeyword(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plKeywordSearch, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

static void startSearchPeople(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plPeopleSearch, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

static void startSearchPower(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plPowerSearch, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

static void startSearchStored(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plStoredSearch, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

static void startSearchChannel(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plChannel, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

static void startSearchCategory(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plCategory, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

static void startSearchMovie(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plMovies, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

static void startSearchNew(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plNewListings, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

static void startSearchTime(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plTime, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

static void startManaged(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    ViewScheduled *viewsched = new ViewScheduled(mainStack);

    if (viewsched->Create())
        mainStack->AddScreen(viewsched);
    else
        delete viewsched;
}

static void startManageRecordingRules(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    ProgramRecPriority *progRecPrior = new ProgramRecPriority(mainStack,
                                                            "ManageRecRules");

    if (progRecPrior->Create())
        mainStack->AddScreen(progRecPrior);
    else
        delete progRecPrior;
}

static void startChannelRecPriorities(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    ChannelRecPriority *chanRecPrior = new ChannelRecPriority(mainStack);

    if (chanRecPrior->Create())
        mainStack->AddScreen(chanRecPrior);
    else
        delete chanRecPrior;
}

static void startCustomPriority(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    CustomPriority *custom = new CustomPriority(mainStack);

    if (custom->Create())
        mainStack->AddScreen(custom);
    else
        delete custom;
}

static void startPlaybackWithGroup(QString recGroup = "")
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    PlaybackBox *pbb = new PlaybackBox(
        mainStack, "playbackbox");

    if (pbb->Create())
    {
        if (!recGroup.isEmpty())
            pbb->setInitialRecGroup(recGroup);

        mainStack->AddScreen(pbb);
    }
    else
        delete pbb;
}

static void startPlayback(void)
{
    startPlaybackWithGroup();
}

static void startPrevious(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack);
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

static void startCustomEdit(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    CustomEdit *custom = new CustomEdit(mainStack);

    if (custom->Create())
        mainStack->AddScreen(custom);
    else
        delete custom;
}

static void startManualSchedule(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    ManualSchedule *mansched= new ManualSchedule(mainStack);

    if (mansched->Create())
        mainStack->AddScreen(mansched);
    else
        delete mansched;
}

static bool isLiveTVAvailable(void)
{
    if (RemoteGetFreeRecorderCount() > 0)
        return true;

    QString msg = qApp->translate("(Common)", "All tuners are currently busy.");

    if (TV::ConfiguredTunerCards() < 1)
        msg = qApp->translate("(Common)", "There are no configured tuners.");

    ShowOkPopup(msg);
    return false;
}

static void startTVNormal(void)
{
    if (isLiveTVAvailable())
        TV::StartTV(NULL, kStartTVNoFlags);
}

static void showStatus(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    StatusBox *statusbox = new StatusBox(mainStack);

    if (statusbox->Create())
        mainStack->AddScreen(statusbox);
    else
        delete statusbox;
}


static void standbyScreen(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    IdleScreen *idlescreen = new IdleScreen(mainStack);

    if (idlescreen->Create())
        mainStack->AddScreen(idlescreen);
    else
        delete idlescreen;
}

static void RunVideoScreen(VideoDialog::DialogType type, bool fromJump = false)
{
    QString message = qApp->translate("(MythFrontendMain)",
                                      "Loading videos ...");

    MythScreenStack *popupStack =
            GetMythMainWindow()->GetStack("popup stack");

    MythUIBusyDialog *busyPopup = new MythUIBusyDialog(message,
            popupStack, "mythvideobusydialog");

    if (busyPopup->Create())
        popupStack->AddScreen(busyPopup, false);

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    VideoDialog::VideoListPtr video_list;
    if (fromJump)
    {
        VideoDialog::VideoListDeathDelayPtr &saved =
                VideoDialog::GetSavedVideoList();
        if (!saved.isNull())
        {
            video_list = saved->GetSaved();
            LOG(VB_GENERAL, LOG_INFO,
                QString("Reusing saved video list because MythVideo was resumed"
                        " within %1ms").arg(VideoListDeathDelay::kDelayTimeMS));
        }
    }

    VideoDialog::BrowseType browse = static_cast<VideoDialog::BrowseType>(
                         gCoreContext->GetNumSetting("mythvideo.db_group_type",
                                                 VideoDialog::BRS_FOLDER));

    if (!video_list)
        video_list = new VideoList;

    VideoDialog *mythvideo =
            new VideoDialog(mainStack, "mythvideo", video_list, type, browse);

    if (mythvideo->Create())
    {
        busyPopup->Close();
        mainStack->AddScreen(mythvideo);
    }
    else
        busyPopup->Close();
}

static void jumpScreenVideoManager() { RunVideoScreen(VideoDialog::DLG_MANAGER, true); }
static void jumpScreenVideoBrowser() { RunVideoScreen(VideoDialog::DLG_BROWSER, true); }
static void jumpScreenVideoTree()    { RunVideoScreen(VideoDialog::DLG_TREE, true);    }
static void jumpScreenVideoGallery() { RunVideoScreen(VideoDialog::DLG_GALLERY, true); }
static void jumpScreenVideoDefault() { RunVideoScreen(VideoDialog::DLG_DEFAULT, true); }

static void RunGallery()
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    GalleryThumbView *galleryView = new GalleryThumbView(mainStack, "galleryview");
    if (galleryView->Create())
    {
        mainStack->AddScreen(galleryView);
        galleryView->Start();
    }
    else
    {
        delete galleryView;
    }
}

static void playDisc()
{
    //
    //  Get the command string to play a DVD
    //

    bool isBD = false;

    QString command_string =
            gCoreContext->GetSetting("mythdvd.DVDPlayerCommand");
    QString bluray_mountpoint =
            gCoreContext->GetSetting("BluRayMountpoint", "/media/cdrom");
    QDir bdtest(bluray_mountpoint + "/BDMV");

    if (bdtest.exists() || MythCDROM::inspectImage(bluray_mountpoint) == MythCDROM::kBluray)
        isBD = true;

    if (isBD)
    {
        GetMythUI()->AddCurrentLocation("playdisc");

        QString filename = QString("bd:/%1").arg(bluray_mountpoint);

        GetMythMainWindow()->HandleMedia("Internal", filename, "", "", "", "",
                                         0, 0, "", 0, "", "", true);

        GetMythUI()->RemoveCurrentLocation();
    }
    else
    {
        QString dvd_device = MediaMonitor::defaultDVDdevice();

        if (dvd_device.isEmpty())
            return;  // User cancelled in the Popup

        GetMythUI()->AddCurrentLocation("playdisc");

        if ((command_string.indexOf("internal", 0, Qt::CaseInsensitive) > -1) ||
            (command_string.length() < 1))
        {
#ifdef Q_OS_MAC
            // Convert a BSD 'leaf' name into a raw device path
            QString filename = "dvd://dev/r";   // e.g. 'dvd://dev/rdisk2'
#elif _WIN32
            QString filename = "dvd:";          // e.g. 'dvd:E\\'
#else
            QString filename = "dvd:/";         // e.g. 'dvd://dev/sda'
#endif
            filename += dvd_device;

            command_string = "Internal";
            GetMythMainWindow()->HandleMedia(command_string, filename, "", "",
                                             "", "", 0, 0, "", 0, "", "", true);
            GetMythUI()->RemoveCurrentLocation();

            return;
        }
        else
        {
            if (command_string.contains("%d"))
            {
                //
                //  Need to do device substitution
                //
                command_string =
                        command_string.replace(QRegExp("%d"), dvd_device);
            }
            gCoreContext->emitTVPlaybackStarted();
            GetMythMainWindow()->PauseIdleTimer(true);
            myth_system(command_string);
            gCoreContext->emitTVPlaybackStopped();
            GetMythMainWindow()->PauseIdleTimer(false);
            if (GetMythMainWindow())
            {
                GetMythMainWindow()->raise();
                GetMythMainWindow()->activateWindow();
                if (GetMythMainWindow()->currentWidget())
                    GetMythMainWindow()->currentWidget()->setFocus();
            }
        }
        GetMythUI()->RemoveCurrentLocation();
    }
}

/////////////////////////////////////////////////
//// Media handlers
/////////////////////////////////////////////////
static void handleDVDMedia(MythMediaDevice *dvd)
{
    if (!dvd)
        return;

    if (!dvd->isUsable()) // This isn't infallible, on some drives both a mount and libudf fail
        return;

    switch (gCoreContext->GetNumSetting("DVDOnInsertDVD", 1))
    {
        case 0 : // Do nothing
            break;
        case 1 : // Display menu (mythdvd)*/
            break;
        case 2 : // play DVD or Blu-ray
            playDisc();
            break;
        default:
            LOG(VB_GENERAL, LOG_ERR,
                "mythdvd main.o: handleMedia() does not know what to do");
    }
}

static void handleGalleryMedia(MythMediaDevice *dev)
{
    // Only handle events for media that are newly mounted
    if (!dev || (dev->getStatus() != MEDIASTAT_MOUNTED
                  && dev->getStatus() != MEDIASTAT_USEABLE))
        return;

    // Check if gallery is already running
    QVector<MythScreenType*> screens;
    GetMythMainWindow()->GetMainStack()->GetScreenList(screens);

    QVector<MythScreenType*>::const_iterator it    = screens.begin();
    QVector<MythScreenType*>::const_iterator itend = screens.end();

    for (; it != itend; ++it)
    {
        if (dynamic_cast<GalleryThumbView*>(*it))
        {
            // Running gallery will receive this event later
            LOG(VB_MEDIA, LOG_INFO, "Main: Ignoring new gallery media - already running");
            return;
        }
    }

    if (gCoreContext->GetNumSetting("GalleryAutoLoad", 0))
    {
        LOG(VB_GUI, LOG_INFO, "Main: Autostarting Gallery for new media");
        GetMythMainWindow()->JumpTo(JUMP_GALLERY_DEFAULT);
    }
    else
        LOG(VB_MEDIA, LOG_INFO, "Main: Ignoring new gallery media - autorun not set");
}

static void TVMenuCallback(void *data, QString &selection)
{
    (void)data;
    QString sel = selection.toLower();

    if (sel.startsWith("settings ") || sel == "video_settings_general")
    {
        GetMythUI()->AddCurrentLocation("Setup");
        gCoreContext->ActivateSettingsCache(false);
        GetMythMainWindow()->HidePainterWindow();
    }

    if (sel == "tv_watch_live")
        startTVNormal();
    else if (sel.startsWith("tv_watch_recording"))
    {
        // use selection here because its case is untouched
        if ((selection.length() > 19) && (selection.mid(18, 1) == " "))
            startPlaybackWithGroup(selection.mid(19));
        else
            startPlayback();
    }
    else if (sel == "tv_schedule")
        startGuide();
    else if (sel == "tv_manualschedule")
        startManualSchedule();
    else if (sel == "tv_custom_record")
        startCustomEdit();
    else if (sel == "tv_fix_conflicts")
        startManaged();
    else if (sel == "tv_manage_recording_rules")
        startManageRecordingRules();
    else if (sel == "tv_progfind")
        startFinder();
    else if (sel == "tv_search_title")
        startSearchTitle();
    else if (sel == "tv_search_keyword")
        startSearchKeyword();
    else if (sel == "tv_search_people")
        startSearchPeople();
    else if (sel == "tv_search_power")
        startSearchPower();
    else if (sel == "tv_search_stored")
        startSearchStored();
    else if (sel == "tv_search_channel")
        startSearchChannel();
    else if (sel == "tv_search_category")
        startSearchCategory();
    else if (sel == "tv_search_movie")
        startSearchMovie();
    else if (sel == "tv_search_new")
        startSearchNew();
    else if (sel == "tv_search_time")
        startSearchTime();
    else if (sel == "tv_previous")
        startPrevious();
    else if (sel == "settings appearance")
    {
        AppearanceSettings *settings = new AppearanceSettings();
        DialogCode res = settings->exec();
        delete settings;

        if (kDialogCodeRejected != res)
        {
            qApp->processEvents();
            GetMythMainWindow()->JumpTo("Reload Theme");
        }
    }
    else if (sel == "settings themechooser")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        ThemeChooser *tp = new ThemeChooser(mainStack);

        if (tp->Create())
            mainStack->AddScreen(tp);
        else
            delete tp;
    }
    else if (sel == "settings setupwizard")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        GeneralSetupWizard *sw = new GeneralSetupWizard(mainStack, "setupwizard");

        if (sw->Create())
            mainStack->AddScreen(sw);
        else
            delete sw;
    }
    else if (sel == "settings grabbers")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        GrabberSettings *gs = new GrabberSettings(mainStack, "grabbersettings");

        if (gs->Create())
            mainStack->AddScreen(gs);
        else
            delete gs;
    }
    else if (sel == "screensetupwizard")
    {
        startAppearWiz();
    }
    else if (sel == "setup_keys")
    {
        startKeysSetup();
    }
    else if (sel == "settings playgroup")
    {
        PlayGroupEditor editor;
        editor.exec();
    }
    else if (sel == "settings general")
    {
        GeneralSettings settings;
        settings.exec();
    }
    else if (sel == "settings audiogeneral")
    {
        AudioGeneralSettings audiosettings;
        audiosettings.exec();
    }
    else if (sel == "settings maingeneral")
    {
        MainGeneralSettings mainsettings;
        mainsettings.exec();
        QStringList strlist( QString("REFRESH_BACKEND") );
        gCoreContext->SendReceiveStringList(strlist);
    }
    else if (sel == "settings playback")
    {
        PlaybackSettings settings;
        settings.exec();
    }
    else if (sel == "settings osd")
    {
        OSDSettings settings;
        settings.exec();
    }
    else if (sel == "settings epg")
    {
        EPGSettings settings;
        settings.exec();
    }
    else if (sel == "settings channelgroups")
    {
        ChannelGroupEditor editor;
        editor.exec();
    }
    else if (sel == "settings generalrecpriorities")
    {
        GeneralRecPrioritiesSettings settings;
        settings.exec();
    }
    else if (sel == "settings channelrecpriorities")
    {
        startChannelRecPriorities();
    }
    else if (sel == "settings custompriority")
    {
        startCustomPriority();
    }
    else if (sel == "system_events")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

        MythSystemEventEditor *msee = new MythSystemEventEditor(
                                    mainStack, "System Event Editor");

        if (msee->Create())
            mainStack->AddScreen(msee);
        else
            delete msee;
    }
    else if (sel == "video_settings_general")
    {
        RunSettingsCompletion::Create(gCoreContext->
                GetNumSetting("VideoAggressivePC", 0));
    }
    else if (sel == "video_settings_player")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

        PlayerSettings *ps = new PlayerSettings(mainStack, "player settings");

        if (ps->Create())
            mainStack->AddScreen(ps);
    }
    else if (sel == "video_settings_metadata")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

        MetadataSettings *ms = new MetadataSettings(mainStack, "metadata settings");

        if (ms->Create())
            mainStack->AddScreen(ms);
    }
    else if (sel == "video_settings_associations")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

        FileAssocDialog *fa = new FileAssocDialog(mainStack, "fa dialog");

        if (fa->Create())
            mainStack->AddScreen(fa);
    }
    else if (sel == "manager")
        RunVideoScreen(VideoDialog::DLG_MANAGER);
    else if (sel == "browser")
        RunVideoScreen(VideoDialog::DLG_BROWSER);
    else if (sel == "listing")
        RunVideoScreen(VideoDialog::DLG_TREE);
    else if (sel == "gallery")
        RunVideoScreen(VideoDialog::DLG_GALLERY);
    else if (sel == "disc_play")
    {
        playDisc();
    }
    else if (sel == "tv_status")
        showStatus();
    else if (sel == "exiting_app_prompt")
        handleExit(true);
    else if (sel == "exiting_app")
        handleExit(false);
    else if (sel == "standby_mode")
        standbyScreen();
    else if (sel == "exiting_menu")
    {
        //ignore
    }
    else
        LOG(VB_GENERAL, LOG_ERR, "Unknown menu action: " + selection);

    if (sel.startsWith("settings ") || sel == "video_settings_general")
    {
        GetMythUI()->RemoveCurrentLocation();

        gCoreContext->ActivateSettingsCache(true);

        // tell the backend the settings may have changed
        gCoreContext->SendMessage("CLEAR_SETTINGS_CACHE");
        // tell the frontend the settings may have changed
        gCoreContext->dispatch(MythEvent(QString("CLEAR_SETTINGS_CACHE")));

        if (sel == "settings general" ||
            sel == "settings generalrecpriorities")
            ScheduledRecording::ReschedulePlace("TVMenuCallback");
        GetMythMainWindow()->ShowPainterWindow();
    }
}

static void handleExit(bool prompt)
{
    if (prompt)
    {
        if (!exitPopup)
            exitPopup = new ExitPrompter();

        exitPopup->handleExit();
    }
    else
        qApp->quit();
}

static bool RunMenu(QString themedir, QString themename)
{
    QByteArray tmp = themedir.toLocal8Bit();
    menu = new MythThemedMenu(QString(tmp.constData()), "mainmenu.xml",
                              GetMythMainWindow()->GetMainStack(), "mainmenu");

    if (menu->foundTheme())
    {
        LOG(VB_GENERAL, LOG_NOTICE, QString("Found mainmenu.xml for theme '%1'")
                .arg(themename));
        menu->setCallback(TVMenuCallback, gContext);
        GetMythMainWindow()->GetMainStack()->AddScreen(menu);
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR,
        QString("Couldn't find mainmenu.xml for theme '%1'") .arg(themename));
    delete menu;
    menu = NULL;

    return false;
}

// If any settings are missing from the database, this will write
// the default values
static void WriteDefaults()
{
    PlaybackSettings ps;
    ps.Load();
    ps.Save();
    OSDSettings os;
    os.Load();
    os.Save();
    GeneralSettings gs;
    gs.Load();
    gs.Save();
    EPGSettings es;
    es.Load();
    es.Save();
    AppearanceSettings as;
    as.Load();
    as.Save();
    MainGeneralSettings mgs;
    mgs.Load();
    mgs.Save();
    GeneralRecPrioritiesSettings grs;
    grs.Load();
    grs.Save();
    VideoGeneralSettings vgs;
    vgs.Load();
    vgs.Save();
}

static int internal_play_media(const QString &mrl, const QString &plot,
                        const QString &title, const QString &subtitle,
                        const QString &director, int season, int episode,
                        const QString &inetref, int lenMins, const QString &year,
                        const QString &id, const bool useBookmark)
{
    int res = -1;

    QFile checkFile(mrl);
    if ((!checkFile.exists() && !mrl.startsWith("dvd:")
         && !mrl.startsWith("bd:")
         && !mrl.startsWith("myth:")
         && !mrl.startsWith("http://")
         && !mrl.startsWith("https://")))
    {
        QString errorText = qApp->translate("(MythFrontendMain)",
            "Failed to open \n '%1' in %2 \n"
            "Check if the video exists")
            .arg(mrl.section('/', -1))
            .arg(mrl.section('/', 0, -2));

        ShowOkPopup(errorText);
        return res;
    }

    ProgramInfo *pginfo = new ProgramInfo(
        mrl, plot, title, subtitle, director, season, episode,
        inetref, lenMins, (year.toUInt()) ? year.toUInt() : 1900,
        id);

    pginfo->SetProgramInfoType(pginfo->DiscoverProgramInfoType());

    bool bookmarkPresent = false;

    if (pginfo->IsVideoDVD())
    {
        DVDInfo *dvd = new DVDInfo(pginfo->GetPlaybackURL());
        if (dvd->IsValid())
        {
            QString name;
            QString serialid;
            if (dvd->GetNameAndSerialNum(name, serialid))
            {
                QStringList fields = pginfo->QueryDVDBookmark(serialid);
                bookmarkPresent = (fields.count() > 0);
            }
        }
        else
        {
            ShowNotificationError(qApp->translate("(MythFrontendMain)",
                                                  "DVD Failure"),
                                                  _Location,
                                                  dvd->GetLastError());
            delete dvd;
            delete pginfo;
            return res;
        }
        delete dvd;
    }
    else if (pginfo->IsVideoBD())
    {
        BDInfo bd(pginfo->GetPlaybackURL());
        if (bd.IsValid())
        {
            QString name;
            QString serialid;
            if (bd.GetNameAndSerialNum(name, serialid))
            {
                QStringList fields = pginfo->QueryBDBookmark(serialid);
                bookmarkPresent = (fields.count() > 0);
            }
        }
        else
        {
            // ToDo: Change string to "BD Failure" after 0.28 is released
            ShowNotificationError(qApp->translate("(MythFrontendMain)",
                                                  "DVD Failure"),
                                                  _Location,
                                                  bd.GetLastError());
            delete pginfo;
            return res;
        }
    }
    else if (pginfo->IsVideo())
        bookmarkPresent = (pginfo->QueryBookmark() > 0);

    if (useBookmark && bookmarkPresent)
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        BookmarkDialog *bookmarkdialog = new BookmarkDialog(pginfo, mainStack);
        if (!bookmarkdialog->Create())
        {
            delete bookmarkdialog;
            delete pginfo;
            return res;
        }
    }
    else
    {
        TV::StartTV(pginfo, kStartTVNoFlags | kStartTVIgnoreBookmark);

        res = 0;

        delete pginfo;
    }

    return res;
}

static void gotoMainMenu(void)
{
    // Reset the selected button to the first item.
    MythThemedMenuState *menu = dynamic_cast<MythThemedMenuState *>
        (GetMythMainWindow()->GetMainStack()->GetTopScreen());
    if (menu)
        menu->m_buttonList->SetItemCurrent(0);
}

// If the theme specified in the DB is somehow broken, try a standard one:
//
static bool resetTheme(QString themedir, const QString &badtheme)
{
    QString themename = DEFAULT_UI_THEME;

    if (badtheme == DEFAULT_UI_THEME)
        themename = FALLBACK_UI_THEME;

    LOG(VB_GENERAL, LOG_WARNING,
        QString("Overriding broken theme '%1' with '%2'")
            .arg(badtheme).arg(themename));

    gCoreContext->OverrideSettingForSession("Theme", themename);
    themedir = GetMythUI()->FindThemeDir(themename);

    MythTranslation::reload();
    gCoreContext->ReInitLocale();
    GetMythUI()->LoadQtConfig();
#if CONFIG_DARWIN
    GetMythMainWindow()->Init(OPENGL2_PAINTER);
#else
    GetMythMainWindow()->Init();
#endif

    GetMythMainWindow()->ReinitDone();

    return RunMenu(themedir, themename);
}

static int reloadTheme(void)
{
    QString themename = gCoreContext->GetSetting("Theme", DEFAULT_UI_THEME);
    QString themedir = GetMythUI()->FindThemeDir(themename);
    if (themedir.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Couldn't find theme '%1'")
                .arg(themename));
        return GENERIC_EXIT_NO_THEME;
    }

    gCoreContext->ReInitLocale();
    MythTranslation::reload();

    GetMythMainWindow()->SetEffectsEnabled(false);

    GetMythUI()->LoadQtConfig();

    if (menu)
    {
        menu->Close();
    }
#if CONFIG_DARWIN
    GetMythMainWindow()->Init(gLoaded ? OPENGL2_PAINTER : QT_PAINTER);
#else
    GetMythMainWindow()->Init();
#endif

    GetMythMainWindow()->ReinitDone();

    GetMythMainWindow()->SetEffectsEnabled(true);

    if (!RunMenu(themedir, themename) && !resetTheme(themedir, themename))
        return GENERIC_EXIT_NO_THEME;

    LCD::SetupLCD();
    if (LCD *lcd = LCD::Get())
    {
        lcd->setupLEDs(RemoteGetRecordingMask);
        lcd->resetServer();
    }

    return 0;
}

static void reloadTheme_void(void)
{
    int err = reloadTheme();
    if (err)
        exit(err);
}

static void setDebugShowBorders(void)
{
    MythPainter *p = GetMythPainter();
    p->SetDebugMode(!p->ShowBorders(), p->ShowTypeNames());

    if (GetMythMainWindow()->GetMainStack()->GetTopScreen())
        GetMythMainWindow()->GetMainStack()->GetTopScreen()->SetRedraw();
}

static void setDebugShowNames(void)
{
    MythPainter *p = GetMythPainter();
    p->SetDebugMode(p->ShowBorders(), !p->ShowTypeNames());

    if (GetMythMainWindow()->GetMainStack()->GetTopScreen())
        GetMythMainWindow()->GetMainStack()->GetTopScreen()->SetRedraw();
}

static void InitJumpPoints(void)
{
     REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Reload Theme"),
         "", "", reloadTheme_void);
     REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Main Menu"),
         "", "", gotoMainMenu);
     REG_JUMPLOC(QT_TRANSLATE_NOOP("MythControls", "Program Guide"),
         "", "", startGuide, "GUIDE");
     REG_JUMPLOC(QT_TRANSLATE_NOOP("MythControls", "Program Finder"),
         "", "", startFinder, "FINDER");
     //REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Search Listings"),
     //    "", "", startSearch);
     REG_JUMPLOC(QT_TRANSLATE_NOOP("MythControls", "Manage Recordings / "
         "Fix Conflicts"), "", "", startManaged, "VIEWSCHEDULED");
     REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Manage Recording Rules"),
         "", "", startManageRecordingRules);
     REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Channel Recording "
         "Priorities"), "", "", startChannelRecPriorities);
     REG_JUMPLOC(QT_TRANSLATE_NOOP("MythControls", "TV Recording Playback"),
         "", "", startPlayback, "JUMPREC");
     REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Live TV"),
         "", "", startTVNormal);
     REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Status Screen"),
         "", "", showStatus);
     REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Previously Recorded"),
         "", "", startPrevious);

     REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Standby Mode"),
         "", "", standbyScreen);

     // Video

     REG_JUMP(JUMP_VIDEO_DEFAULT, QT_TRANSLATE_NOOP("MythControls",
         "The Video Default View"), "", jumpScreenVideoDefault);
     REG_JUMP(JUMP_VIDEO_MANAGER, QT_TRANSLATE_NOOP("MythControls",
         "The Video Manager"), "", jumpScreenVideoManager);
     REG_JUMP(JUMP_VIDEO_BROWSER, QT_TRANSLATE_NOOP("MythControls",
         "The Video Browser"), "", jumpScreenVideoBrowser);
     REG_JUMP(JUMP_VIDEO_TREE, QT_TRANSLATE_NOOP("MythControls",
         "The Video Listings"), "", jumpScreenVideoTree);
     REG_JUMP(JUMP_VIDEO_GALLERY, QT_TRANSLATE_NOOP("MythControls",
         "The Video Gallery"), "", jumpScreenVideoGallery);
     REG_JUMP("Play Disc", QT_TRANSLATE_NOOP("MythControls",
         "Play an Optical Disc"), "", playDisc);

     // Gallery

     REG_JUMP(JUMP_GALLERY_DEFAULT, QT_TRANSLATE_NOOP("MythControls",
         "Image Gallery"), "", RunGallery);

     REG_JUMPEX(QT_TRANSLATE_NOOP("MythControls", "Toggle Show Widget Borders"),
         "", "", setDebugShowBorders, false);
     REG_JUMPEX(QT_TRANSLATE_NOOP("MythControls", "Toggle Show Widget Names"),
         "", "", setDebugShowNames, false);
     REG_JUMPEX(QT_TRANSLATE_NOOP("MythControls", "Reset All Keys"),
         QT_TRANSLATE_NOOP("MythControls", "Reset all keys to defaults"),
         "", resetAllKeys, false);
}

static void ReloadJumpPoints(void)
{
    MythMainWindow *mainWindow = GetMythMainWindow();
    mainWindow->ClearAllJumps();
    InitJumpPoints();
}

static void InitKeys(void)
{
     REG_KEY("Video","PLAYALT", QT_TRANSLATE_NOOP("MythControls",
         "Play selected item in alternate player"), "ALT+P");
     REG_KEY("Video","FILTER", QT_TRANSLATE_NOOP("MythControls",
         "Open video filter dialog"), "F");
     REG_KEY("Video","INCPARENT", QT_TRANSLATE_NOOP("MythControls",
         "Increase Parental Level"), "],},F11");
     REG_KEY("Video","DECPARENT", QT_TRANSLATE_NOOP("MythControls",
         "Decrease Parental Level"), "[,{,F10");
     REG_KEY("Video","INCSEARCH", QT_TRANSLATE_NOOP("MythControls",
         "Show Incremental Search Dialog"), "Ctrl+S");
     REG_KEY("Video","DOWNLOADDATA", QT_TRANSLATE_NOOP("MythControls",
         "Download metadata for current item"), "W");
     REG_KEY("Video","ITEMDETAIL", QT_TRANSLATE_NOOP("MythControls",
         "Display Item Detail Popup"), "");

     // Gallery keybindings
     REG_KEY("Images", "PLAY", QT_TRANSLATE_NOOP("MythControls",
         "Start/Stop Slideshow"), "P");
     REG_KEY("Images", "RECURSIVESHOW", QT_TRANSLATE_NOOP("MythControls",
         "Start Recursive Slideshow"), "R");
     REG_KEY("Images", "ROTRIGHT", QT_TRANSLATE_NOOP("MythControls",
         "Rotate image right 90 degrees"), "],3");
     REG_KEY("Images", "ROTLEFT", QT_TRANSLATE_NOOP("MythControls",
         "Rotate image left 90 degrees"), "[,1");
     REG_KEY("Images", "FLIPHORIZONTAL", QT_TRANSLATE_NOOP("MythControls",
         "Flip image horizontally"), "");
     REG_KEY("Images", "FLIPVERTICAL", QT_TRANSLATE_NOOP("MythControls",
         "Flip image vertically"), "");
     REG_KEY("Images", "ZOOMOUT", QT_TRANSLATE_NOOP("MythControls",
         "Zoom image out"), "7");
     REG_KEY("Images", "ZOOMIN", QT_TRANSLATE_NOOP("MythControls",
         "Zoom image in"), "9");
     REG_KEY("Images", "FULLSIZE", QT_TRANSLATE_NOOP("MythControls",
         "Full-size (un-zoom) image"), "0");
     REG_KEY("Images", "MARK", QT_TRANSLATE_NOOP("MythControls",
         "Mark image"), "T");
     REG_KEY("Images", "SCROLLUP", QT_TRANSLATE_NOOP("MythControls",
         "Scroll image up"), "2");
     REG_KEY("Images", "SCROLLLEFT", QT_TRANSLATE_NOOP("MythControls",
         "Scroll image left"), "4");
     REG_KEY("Images", "SCROLLRIGHT", QT_TRANSLATE_NOOP("MythControls",
         "Scroll image right"), "6");
     REG_KEY("Images", "SCROLLDOWN", QT_TRANSLATE_NOOP("MythControls",
         "Scroll image down"), "8");
     REG_KEY("Images", "RECENTER", QT_TRANSLATE_NOOP("MythControls",
         "Recenter image"), "5");
     REG_KEY("Images", "COVER", QT_TRANSLATE_NOOP("MythControls",
         "Set or clear cover image"), "C");
}

static void ReloadKeys(void)
{
    GetMythMainWindow()->ClearKeyContext("Video");
    InitKeys();

    TV::ReloadKeys();
}

static void SetFuncPtrs(void)
{
    TV::SetFuncPtr("playbackbox", (void *)PlaybackBox::RunPlaybackBox);
    TV::SetFuncPtr("viewscheduled", (void *)ViewScheduled::RunViewScheduled);
    TV::SetFuncPtr("programguide", (void *)GuideGrid::RunProgramGuide);
    TV::SetFuncPtr("programfinder", (void *)RunProgramFinder);
    TV::SetFuncPtr("scheduleeditor", (void *)ScheduleEditor::RunScheduleEditor);
}

/**
 *  \brief Deletes all key bindings and jump points for this host
 */
static void clearAllKeys(void)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("DELETE FROM keybindings "
                  "WHERE hostname = :HOSTNAME;");
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());
    if (!query.exec())
        MythDB::DBError("Deleting keybindings", query);
    query.prepare("DELETE FROM jumppoints "
                  "WHERE hostname = :HOSTNAME;");
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());
    if (!query.exec())
        MythDB::DBError("Deleting jumppoints", query);
}

/**
 *  \brief Reset this host's key bindings and jump points to default values
 */
static void resetAllKeys(void)
{
    clearAllKeys();
    // Reload MythMainWindow bindings
    GetMythMainWindow()->ReloadKeys();
    // Reload Jump Points
    ReloadJumpPoints();
    // Reload mythfrontend and TV bindings
    ReloadKeys();
}

static int internal_media_init()
{
    REG_MEDIAPLAYER("Internal", QT_TRANSLATE_NOOP("MythControls",
        "MythTV's native media player."), internal_play_media);
    REG_MEDIA_HANDLER(QT_TRANSLATE_NOOP("MythControls",
        "MythDVD DVD Media Handler"), "", "", handleDVDMedia,
        MEDIATYPE_DVD, QString::null);
    REG_MEDIA_HANDLER(QT_TRANSLATE_NOOP("MythControls",
        "MythImage Media Handler 1/2"), "", "", handleGalleryMedia,
        MEDIATYPE_DATA | MEDIATYPE_MIXED, QString::null);

    QStringList extensions(ImageAdapterBase::SupportedImages()
                           + ImageAdapterBase::SupportedVideos());

    REG_MEDIA_HANDLER(QT_TRANSLATE_NOOP("MythControls",
        "MythImage Media Handler 2/2"), "", "", handleGalleryMedia,
        MEDIATYPE_MGALLERY | MEDIATYPE_MVIDEO, extensions.join(","));
    return 0;
}

static void CleanupMyOldInUsePrograms(void)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("DELETE FROM inuseprograms "
                  "WHERE hostname = :HOSTNAME and recusage = 'player' ;");
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());
    if (!query.exec())
        MythDB::DBError("CleanupMyOldInUsePrograms", query);
}

static bool WasAutomaticStart(void)
{
    bool autoStart = false;

    // Is backend running?
    //
    if( gCoreContext->BackendIsRunning() )
    {
        QDateTime startupTime = QDateTime();

        if( gCoreContext->IsMasterHost() )
        {
            QString s = gCoreContext->GetSetting("MythShutdownWakeupTime", "");
            if (s.length())
                startupTime = MythDate::fromString(s);

            // if we don't have a valid startup time assume we were started manually
            if (startupTime.isValid())
            {
                int startupSecs = gCoreContext->GetNumSetting("StartupSecsBeforeRecording");
                // If we started within 'StartupSecsBeforeRecording' OR 15 minutes
                // of the saved wakeup time assume we either started automatically
                // to record, to obtain guide data or or for a
                // daily wakeup/shutdown period
                if (abs(startupTime.secsTo(MythDate::current())) <
                    max(startupSecs, 15 * 60))
                {
                    LOG(VB_GENERAL, LOG_INFO,
                        "Close to auto-start time, AUTO-Startup assumed");

                    QString str = gCoreContext->GetSetting("MythFillSuggestedRunTime");
                    QDateTime guideRunTime = MythDate::fromString(str);
                    if (guideRunTime.secsTo(MythDate::current()) <
                        max(startupSecs, 15 * 60))
                    {
                        LOG(VB_GENERAL, LOG_INFO,
                            "Close to MythFillDB suggested run time, AUTO-Startup to fetch guide data?");
                    }
                    autoStart = true;
                }
                else
                    LOG(VB_GENERAL, LOG_DEBUG,
                        "NOT close to auto-start time, USER-initiated startup assumed");
            }
        }
        else
        {
            QString wakeupCmd = gCoreContext->GetSetting("WakeUpCommand");

            // A slave backend that has no wakeup command cannot be woken
            // automatically so can be ignored.
            if (!wakeupCmd.isEmpty())
            {
                ProgramList progList;
                bool        bConflicts;
                QDateTime   nextRecordingStart;

                if (LoadFromScheduler(progList, bConflicts))
                {
                    // Find the first recording to be recorded
                    // on this machine
                    QString hostname = gCoreContext->GetHostName();
                    ProgramList::const_iterator it = progList.begin();
                    for (; it != progList.end(); ++it)
                    {
                        if (((*it)->GetRecordingStatus() == RecStatus::WillRecord ||
                             (*it)->GetRecordingStatus() == RecStatus::Pending) &&
                            ((*it)->GetHostname() == hostname) &&
                            (nextRecordingStart.isNull() ||
                             nextRecordingStart > (*it)->GetRecordingStartTime()))
                        {
                            nextRecordingStart = (*it)->GetRecordingStartTime();
                        }
                    }

                    if (!nextRecordingStart.isNull() &&
                        (abs(nextRecordingStart.secsTo(MythDate::current())) < (4 * 60)))
                    {
                        LOG(VB_GENERAL, LOG_INFO,
                            "Close to start time, AUTO-Startup assumed");

                        // If we started within 4 minutes of the next recording,
                        // we almost certainly started automatically.
                        autoStart = true;
                    }
                    else
                        LOG(VB_GENERAL, LOG_DEBUG,
                            "NOT close to auto-start time, USER-initiated startup assumed");

                }
            }
        }
    }

    return autoStart;
}

// from https://www.raspberrypi.org/forums/viewtopic.php?f=33&t=16897
// The old way of revoking root with setuid(getuid()) 
// causes system hang in certain cases on raspberry pi

static int revokeRoot (void)
{
  if (getuid () == 0 && geteuid () == 0)      // Really running as root
    return 0;

  if (geteuid () == 0)                  // Running setuid root
     return seteuid (getuid ()) ;               // Change effective uid to the uid of the caller
  return 0;
}


int main(int argc, char **argv)
{
    bool bPromptForBackend    = false;
    bool bBypassAutoDiscovery = false;

#ifdef Q_OS_ANDROID
    // extra for 0 termination
    char *newargv[argc+4+1];
    memset(newargv, 0, sizeof(newargv));
    memcpy(newargv, argv, sizeof(char*) * argc);
    //newargv[argc++] = "-v";
    //newargv[argc++] = "general,gui,playback";
    //newargv[argc++] = "--loglevel";
    //newargv[argc++] = "debug";
    argv = &newargv[0];
#endif

    MythFrontendCommandLineParser cmdline;
    if (!cmdline.Parse(argc, argv))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (cmdline.toBool("showhelp"))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_OK;
    }

    if (cmdline.toBool("showversion"))
    {
        cmdline.PrintVersion();
        return GENERIC_EXIT_OK;
    }

    CleanupGuard callCleanup(cleanup);

#ifdef Q_OS_MAC
    // Without this, we can't set focus to any of the CheckBoxSetting, and most
    // of the MythPushButton widgets, and they don't use the themed background.
    QApplication::setDesktopSettingsAware(false);
#endif
#ifdef Q_OS_LINUX
    // This makes Xlib calls thread-safe which seems to be required for hardware
    // accelerated Flash playback to work without causing mythfrontend to abort.
    QApplication::setAttribute(Qt::AA_X11InitThreads);
#endif
#ifdef Q_OS_ANDROID
    //QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#endif
#if QT_VERSION >= 0x050300
    QApplication::setSetuidAllowed(true);
#endif
    new QApplication(argc, argv);
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHFRONTEND);

#ifdef Q_OS_MAC
    QString path = QCoreApplication::applicationDirPath();
    setenv("PYTHONPATH",
           QString("%1/../Resources/lib/python2.6/site-packages:%2")
           .arg(path)
           .arg(QProcessEnvironment::systemEnvironment().value("PYTHONPATH"))
           .toUtf8().constData(), 1);
#endif

#ifndef _WIN32
    QList<int> signallist;
    signallist << SIGINT << SIGTERM << SIGSEGV << SIGABRT << SIGBUS << SIGFPE
               << SIGILL;
#if ! CONFIG_DARWIN
    signallist << SIGRTMIN;
#endif
    SignalHandler::Init(signallist);
    SignalHandler::SetHandler(SIGUSR1, handleSIGUSR1);
    SignalHandler::SetHandler(SIGUSR2, handleSIGUSR2);
    SignalHandler::SetHandler(SIGHUP, logSigHup);
#endif

    int retval;
    if ((retval = cmdline.ConfigureLogging()) != GENERIC_EXIT_OK)
        return retval;

    bool ResetSettings = false;

    if (cmdline.toBool("prompt"))
        bPromptForBackend = true;
    if (cmdline.toBool("noautodiscovery"))
        bBypassAutoDiscovery = true;

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        cerr << "Unable to ignore SIGPIPE\n";

    if (!cmdline.toString("display").isEmpty())
    {
        MythUIHelper::SetX11Display(cmdline.toString("display"));
    }

    if (!cmdline.toString("geometry").isEmpty())
    {
        MythUIHelper::ParseGeometryOverride(cmdline.toString("geometry"));
    }

    gContext = new MythContext(MYTH_BINARY_VERSION);

    if (!gContext->Init(true, bPromptForBackend, bBypassAutoDiscovery))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to init MythContext, exiting.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }
    gCoreContext->SetAsFrontend(true);

    cmdline.ApplySettingsOverride();

    if (!GetMythDB()->HaveSchema())
    {
        if (!InitializeMythSchema())
            return GENERIC_EXIT_DB_ERROR;
    }

    if (cmdline.toBool("reset"))
        ResetSettings = true;

    if (!cmdline.toBool("noupnp"))
    {
        g_pUPnp  = new MediaRenderer();
        if (!g_pUPnp->isInitialized())
        {
            delete g_pUPnp;
            g_pUPnp = NULL;
        }
    }

    QString fileprefix = GetConfDir();

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    if (ResetSettings)
    {
        AppearanceSettings as;
        as.Save();

        gCoreContext->SaveSetting("Theme", DEFAULT_UI_THEME);
        gCoreContext->GetDB()->ClearSetting("Language");
        gCoreContext->GetDB()->ClearSettingOnHost("Language", NULL);
        gCoreContext->GetDB()->ClearSetting("Country");
        gCoreContext->GetDB()->ClearSettingOnHost("Country", NULL);

        LOG(VB_GENERAL, LOG_NOTICE, "Appearance settings and language have "
                                    "been reset to defaults. You will need to "
                                    "restart the frontend.");

        return GENERIC_EXIT_OK;
    }

#if QT_VERSION >= QT_VERSION_CHECK(5,3,0)
    qApp->setSetuidAllowed(true);
#endif

    if (revokeRoot() != 0)
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to revokeRoot(), exiting.");
        return GENERIC_EXIT_NOT_OK;
    }

#ifdef USING_LIBDNS_SD
    // this needs to come after gCoreContext has been initialised
    // (for hostname) - hence it is not in MediaRenderer
    QScopedPointer<BonjourRegister> bonjour(new BonjourRegister());
    if (bonjour.data())
    {
        QByteArray dummy;
        int port = gCoreContext->GetNumSetting("UPnP/MythFrontend/ServicePort", 6547);
        QByteArray name("Mythfrontend on ");
        name.append(gCoreContext->GetHostName());
        bonjour->Register(port, "_mythfrontend._tcp",
                                 name, dummy);
    }
#endif

    LCD::SetupLCD();
    if (LCD *lcd = LCD::Get())
        lcd->setupLEDs(RemoteGetRecordingMask);

    MythTranslation::load("mythfrontend");

    QString themename = gCoreContext->GetSetting("Theme", DEFAULT_UI_THEME);

    QString themedir = GetMythUI()->FindThemeDir(themename);
    if (themedir.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Couldn't find theme '%1'")
                .arg(themename));
        return GENERIC_EXIT_NO_THEME;
    }

    GetMythUI()->LoadQtConfig();

    themename = gCoreContext->GetSetting("Theme", DEFAULT_UI_THEME);
    themedir = GetMythUI()->FindThemeDir(themename);
    if (themedir.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Couldn't find theme '%1'")
                .arg(themename));
        return GENERIC_EXIT_NO_THEME;
    }

    MythMainWindow *mainWindow = GetMythMainWindow();
#if CONFIG_DARWIN
    mainWindow->Init(QT_PAINTER);
#else
    mainWindow->Init();
#endif
    mainWindow->setWindowTitle(qApp->translate("(MythFrontendMain)",
                                               "MythTV Frontend",
                                               "Main window title"));

#ifdef USING_AIRPLAY
    if (gCoreContext->GetNumSetting("AirPlayEnabled", true))
    {
        MythRAOPDevice::Create();
        if (!gCoreContext->GetNumSetting("AirPlayAudioOnly", false))
        {
            MythAirplayServer::Create();
        }
    }
#endif

    // We must reload the translation after a language change and this
    // also means clearing the cached/loaded theme strings, so reload the
    // theme which also triggers a translation reload
    if (LanguageSelection::prompt())
    {
        if (!reloadTheme())
            return GENERIC_EXIT_NO_THEME;
    }

    if (!UpgradeTVDatabaseSchema(false))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Couldn't upgrade database to new schema, exiting.");
        return GENERIC_EXIT_DB_OUTOFDATE;
    }

    WriteDefaults();

    // Refresh Global/Main Menu keys after DB update in case there was no DB
    // when they were written originally
    mainWindow->ReloadKeys();

    InitJumpPoints();
    InitKeys();
    TV::InitKeys();
    SetFuncPtrs();

    internal_media_init();

    CleanupMyOldInUsePrograms();

    setHttpProxy();

    pmanager = new MythPluginManager();
    gCoreContext->SetPluginManager(pmanager);

    MediaMonitor *mon = MediaMonitor::GetMediaMonitor();
    if (mon)
    {
        mon->StartMonitoring();
        mainWindow->installEventFilter(mon);
    }

    NetworkControl *networkControl = NULL;
    if (gCoreContext->GetNumSetting("NetworkControlEnabled", 0))
    {
        int port = gCoreContext->GetNumSetting("NetworkControlPort", 6546);
        networkControl = new NetworkControl();
        if (!networkControl->listen(port))
            LOG(VB_GENERAL, LOG_ERR,
                QString("NetworkControl failed to bind to port %1.")
                   .arg(port));
    }

#if CONFIG_DARWIN
    GetMythMainWindow()->SetEffectsEnabled(false);
    GetMythMainWindow()->Init(OPENGL2_PAINTER);
    GetMythMainWindow()->ReinitDone();
    GetMythMainWindow()->SetEffectsEnabled(true);
    gLoaded = true;
#endif
    if (!RunMenu(themedir, themename) && !resetTheme(themedir, themename))
    {
        return GENERIC_EXIT_NO_THEME;
    }
    ThemeUpdateChecker *themeUpdateChecker = NULL;
    if (gCoreContext->GetNumSetting("ThemeUpdateNofications", 1))
        themeUpdateChecker = new ThemeUpdateChecker();

    MythSystemEventHandler *sysEventHandler = new MythSystemEventHandler();

    BackendConnectionManager bcm;

    PreviewGeneratorQueue::CreatePreviewGeneratorQueue(
        PreviewGenerator::kRemote, 50, 60);

    HouseKeeper *housekeeping = new HouseKeeper();
#ifdef __linux__
 #ifdef CONFIG_BINDINGS_PYTHON
    housekeeping->RegisterTask(new HardwareProfileTask());
 #endif
#endif
    housekeeping->Start();


    if (cmdline.toBool("runplugin"))
    {
        QStringList plugins = pmanager->EnumeratePlugins();

        if (plugins.contains(cmdline.toString("runplugin")))
            pmanager->run_plugin(cmdline.toString("runplugin"));
        else if (plugins.contains("myth" + cmdline.toString("runplugin")))
            pmanager->run_plugin("myth" + cmdline.toString("runplugin"));
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Invalid plugin name supplied on command line: '%1'")
                    .arg(cmdline.toString("runplugin")));
            LOG(VB_GENERAL, LOG_ERR,
                QString("Available plugins: %1")
                    .arg(plugins.join(", ")));
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
    }
    else if (cmdline.toBool("jumppoint"))
    {
        MythMainWindow *mmw = GetMythMainWindow();

        if (mmw->DestinationExists(cmdline.toString("jumppoint")))
            mmw->JumpTo(cmdline.toString("jumppoint"));
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Invalid jump point supplied on the command line: %1")
                    .arg(cmdline.toString("jumppoint")));
            LOG(VB_GENERAL, LOG_ERR,
                QString("Available jump points: %2")
                    .arg(mmw->EnumerateDestinations().join(", ")));
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
    }

    if (WasAutomaticStart())
    {
        // We appear to have been started automatically
        // so enter standby so that the machine can
        // shutdown again as soon as possible if necessary.
        standbyScreen();
    }

    int ret = qApp->exec();

    PreviewGeneratorQueue::TeardownPreviewGeneratorQueue();

    delete housekeeping;

    if (themeUpdateChecker)
        delete themeUpdateChecker;

    delete sysEventHandler;

    pmanager->DestroyAllPlugins();

    if (mon)
        mon->deleteLater();

    delete networkControl;

    return ret;

}

void handleSIGUSR1(void)
{
    LOG(VB_GENERAL, LOG_INFO, "Reloading theme");
    gCoreContext->SendMessage("CLEAR_SETTINGS_CACHE");
    gCoreContext->ActivateSettingsCache(false);
    GetMythMainWindow()->JumpTo("Reload Theme");
    gCoreContext->ActivateSettingsCache(true);
}

void handleSIGUSR2(void)
{
    LOG(VB_GENERAL, LOG_INFO, "Restarting LIRC handler");
    GetMythMainWindow()->StartLIRC();
}

#include "main.moc"
/* vim: set expandtab tabstop=4 shiftwidth=4: */
