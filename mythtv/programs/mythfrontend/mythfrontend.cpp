// C/C++
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include "zlib.h"

// Qt
#include <QtGlobal>
#ifdef Q_OS_ANDROID
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#include <QtAndroidExtras>
#else
#include <QCoreApplication>
#include <QJniObject>
#define QAndroidJniObject QJniObject
#endif
#endif
#include <QApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMap>
#ifdef Q_OS_DARWIN
#include <QProcessEnvironment>
#endif
#include <QTimer>

// MythTV
#include "libmyth/audio/audiooutput.h"
#include "libmythui/langsettings.h"
#include "libmyth/mythcontext.h"
#include "libmythui/standardsettings.h"
#include "libmythbase/cleanupguard.h"
#include "libmythbase/compat.h"  // For SIG* on MinGW
#include "libmythbase/exitcodes.h"
#include "libmythbase/hardwareprofile.h"
#include "libmythbase/lcddevice.h"
#include "libmythbase/mythcdrom.h"
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythmiscutil.h"
#include "libmythbase/mythplugin.h"
#include "libmythbase/mythsystemlegacy.h"
#include "libmythbase/mythtranslation.h"
#include "libmythbase/mythversion.h"
#include "libmythbase/programinfo.h"
#include "libmythbase/referencecounter.h"
#include "libmythbase/remoteutil.h"
#include "libmythbase/signalhandling.h"
#include "libmythmetadata/cleanup.h"
#include "libmythmetadata/globals.h"
#include "libmythtv/channelutil.h"
#include "libmythtv/dbcheck.h"
#include "libmythtv/mythsystemevent.h"
#include "libmythtv/playgroup.h"
#include "libmythtv/previewgeneratorqueue.h"
#include "libmythtv/scheduledrecording.h"
#include "libmythtv/tv.h"
#include "libmythtv/tvremoteutil.h"
#include "libmythui/mediamonitor.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/myththemedmenu.h"
#include "libmythui/mythuihelper.h"
#include "libmythupnp/taskqueue.h"

// MythFrontend
#include "audiogeneralsettings.h"
#include "backendconnectionmanager.h"
#include "channelrecpriority.h"
#include "customedit.h"
#include "custompriority.h"
#include "exitprompt.h"
#include "globalsettings.h"
#include "grabbersettings.h"
#include "guidegrid.h"
#include "idlescreen.h"
#include "manualschedule.h"
#include "mediarenderer.h"
#include "mythcontrols.h"
#include "mythfrontend_commandlineparser.h"
#include "networkcontrol.h"
#include "playbackbox.h"
#include "prevreclist.h"
#include "progfind.h"
#include "proglist.h"
#include "programrecpriority.h"
#include "scheduleeditor.h"
#include "settingshelper.h"
#include "setupwizard_general.h"
#include "statusbox.h"
#include "themechooser.h"
#include "viewscheduled.h"

// Video
#include "videodlg.h"
#include "videoglobalsettings.h"
#include "videofileassoc.h"
#include "videoplayersettings.h"
#include "videometadatasettings.h"
#include "videolist.h"

// Gallery
#include "gallerythumbview.h"

// DVD & Bluray
#include "libmythtv/Bluray/mythbdbuffer.h"
#include "libmythtv/Bluray/mythbdinfo.h"
#include "libmythtv/DVD/mythdvdbuffer.h"

// AirPlay
#ifdef USING_AIRPLAY
#include "libmythtv/AirPlay/mythairplayserver.h"
#include "libmythtv/AirPlay/mythraopdevice.h"
#endif

#ifdef USING_LIBDNS_SD
#include <QScopedPointer>
#include "libmythbase/bonjourregister.h"
#endif
#if CONFIG_SYSTEMD_NOTIFY
#include <systemd/sd-daemon.h>
static inline void fe_sd_notify(const char *str) { sd_notify(0, str); };
#else
static inline void fe_sd_notify(const char */*str*/) {};
#endif

#include "libmythbase/http/mythhttproot.h"
#include "libmythbase/http/mythhttpinstance.h"
#include "services/mythfrontendservice.h"
#include "libmythbase/http/mythhttprewrite.h"

static MythThemedMenu *g_menu;

static MediaRenderer  *g_pUPnp   = nullptr;
static MythPluginManager *g_pmanager = nullptr;

static SettingsHelper *g_settingsHelper = nullptr;

static void handleExit(bool prompt);
static void resetAllKeys(void);
void handleSIGUSR1(void);
void handleSIGUSR2(void);

#ifdef Q_OS_DARWIN
static bool gLoaded = false;
#endif

static const QString sLocation = QCoreApplication::translate("(Common)",
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
                connect(&m_plcc, &ParentalLevelChangeChecker::SigResultReady,
                        this, &RunSettingsCompletion::OnPasswordResultReady);
                m_plcc.Check(ParentalLevel::plMedium, ParentalLevel::plHigh);
            }
            else
            {
                OnPasswordResultReady(true, ParentalLevel::plHigh);
            }
        }

        ~RunSettingsCompletion() override = default;

      private slots:
        void OnPasswordResultReady(bool passwordValid,
                ParentalLevel::Level newLevel)
        {
            (void) newLevel;

            if (passwordValid)
            {
                MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
                auto *ssd =
                    new StandardSettingDialog(mainStack, "videogeneralsettings",
                                              new VideoGeneralSettings());

                if (ssd->Create())
                {
                    mainStack->AddScreen(ssd);
                }
                else
                {
                    delete ssd;
                }
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

    /// This dialog is used when playing something from the "Watch
    /// Videos" page. Playing from the "Watch Recordings" page uses
    /// the code in PlaybackBox::createPlayFromMenu.
    class BookmarkDialog : MythScreenType
    {
        Q_DECLARE_TR_FUNCTIONS(BookmarkDialog)

      public:
        BookmarkDialog(ProgramInfo *pginfo, MythScreenStack *parent,
                       bool bookmarkPresent, bool lastPlayPresent) :
                MythScreenType(parent, "bookmarkdialog"),
                m_pgi(pginfo),
                m_bookmarked(bookmarkPresent),
                m_lastPlayed(lastPlayPresent),
                m_btnPlayBookmark(tr("Play from bookmark")),
                m_btnClearBookmark(tr("Clear bookmark")),
                m_btnPlayBegin(tr("Play from beginning")),
                m_btnPlayLast(tr("Play from last played position")),
                m_btnClearLast(tr("Clear last played position"))       {
        }

        bool Create() override // MythScreenType
        {
            QString msg = tr("DVD/Video contains a bookmark");

            m_videoDlg = dynamic_cast<VideoDialog*>(GetScreenStack()->GetTopScreen());
            auto *popup = new MythDialogBox(msg, GetScreenStack(), "bookmarkdialog");
            if (!popup->Create())
            {
                delete popup;
                return false;
            }

            GetScreenStack()->AddScreen(popup);

            popup->SetReturnEvent(this, "bookmarkdialog");
            if (m_lastPlayed)
                popup->AddButton(m_btnPlayLast);
            if (m_bookmarked)
                popup->AddButton(m_btnPlayBookmark);
            popup->AddButton(m_btnPlayBegin);
            if (m_lastPlayed)
                popup->AddButton(m_btnClearLast);
            if (m_bookmarked)
                popup->AddButton(m_btnClearBookmark);
            return true;
        }

        void customEvent(QEvent *event) override // MythUIType
        {
            if (event->type() != DialogCompletionEvent::kEventType)
                return;

            auto *dce = (DialogCompletionEvent*)(event);
            QString buttonText = dce->GetResultText();

            if (dce->GetId() != "bookmarkdialog")
                return;

            if (buttonText == m_btnPlayLast)
                TV::StartTV(m_pgi, kStartTVNoFlags);
            else if (buttonText == m_btnPlayBookmark)
                TV::StartTV(m_pgi, kStartTVIgnoreLastPlayPos );
            else if (buttonText == m_btnPlayBegin)
                TV::StartTV(m_pgi, kStartTVIgnoreLastPlayPos | kStartTVIgnoreBookmark);
            else if (buttonText == m_btnClearBookmark)
            {
                m_pgi->SaveBookmark(0);
                if (m_videoDlg)
                {
                    m_videoDlg->playbackStateChanged(m_pgi->GetBasename());
                }
            }
            else if (buttonText == m_btnClearLast)
            {
                m_pgi->SaveLastPlayPos(0);
                if (m_videoDlg)
                {
                    m_videoDlg->playbackStateChanged(m_pgi->GetBasename());
                }
            }
            delete m_pgi;
        }

      private:
        ProgramInfo* m_pgi              {nullptr};
        bool         m_bookmarked       {false};
        bool         m_lastPlayed       {false};
        QString      m_btnPlayBookmark;
        QString      m_btnClearBookmark;
        QString      m_btnPlayBegin;
        QString      m_btnPlayLast;
        QString      m_btnClearLast;
        VideoDialog  *m_videoDlg        {nullptr};
    };

    void cleanup()
    {
        QCoreApplication::processEvents();
        DestroyMythMainWindow();
#ifdef USING_AIRPLAY
        MythRAOPDevice::Cleanup();
        MythAirplayServer::Cleanup();
#endif

        AudioOutput::Cleanup();

        if (g_pUPnp)
        {
            // This takes a few seconds, so inform the user:
            LOG(VB_GENERAL, LOG_INFO, "Shutting down UPnP client...");
            delete g_pUPnp;
            g_pUPnp = nullptr;
        }

        if (g_pmanager)
        {
            delete g_pmanager;
            g_pmanager = nullptr;
        }

        if (g_settingsHelper)
        {
            delete g_settingsHelper;
            g_settingsHelper = nullptr;
        }

        delete gContext;
        gContext = nullptr;

        ReferenceCounter::PrintDebug();

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
    {
        ShowOkPopup(QCoreApplication::translate("(MythFrontendMain)",
                    "The ScreenSetupWizard cannot be used while "
                    "mythfrontend is operating in windowed mode."));
    }
    else
    {
        auto *wizard = new MythSystemLegacy(
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
        wizard = nullptr;
    }

    if (reload)
        GetMythMainWindow()->JumpTo("Reload Theme");
}

static void startKeysSetup()
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *mythcontrols = new MythControls(mainStack, "mythcontrols");

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
    GuideGrid::RunProgramGuide(chanid, channum, startTime, nullptr, false, true, -2);
}

static void startFinder(void)
{
    RunProgramFinder();
}

static void startSearchTitle(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *pl = new ProgLister(mainStack, plTitleSearch, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

static void startSearchKeyword(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *pl = new ProgLister(mainStack, plKeywordSearch, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

static void startSearchPeople(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *pl = new ProgLister(mainStack, plPeopleSearch, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

static void startSearchPower(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *pl = new ProgLister(mainStack, plPowerSearch, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

static void startSearchStored(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *pl = new ProgLister(mainStack, plStoredSearch, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

static void startSearchChannel(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *pl = new ProgLister(mainStack, plChannel, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

static void startSearchCategory(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *pl = new ProgLister(mainStack, plCategory, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

static void startSearchMovie(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *pl = new ProgLister(mainStack, plMovies, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

static void startSearchNew(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *pl = new ProgLister(mainStack, plNewListings, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

static void startSearchTime(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *pl = new ProgLister(mainStack, plTime, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

static void startManaged(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *viewsched = new ViewScheduled(mainStack);

    if (viewsched->Create())
        mainStack->AddScreen(viewsched);
    else
        delete viewsched;
}

static void startManageRecordingRules(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *progRecPrior = new ProgramRecPriority(mainStack, "ManageRecRules");

    if (progRecPrior->Create())
        mainStack->AddScreen(progRecPrior);
    else
        delete progRecPrior;
}

static void startChannelRecPriorities(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *chanRecPrior = new ChannelRecPriority(mainStack);

    if (chanRecPrior->Create())
        mainStack->AddScreen(chanRecPrior);
    else
        delete chanRecPrior;
}

static void startCustomPriority(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *custom = new CustomPriority(mainStack);

    if (custom->Create())
        mainStack->AddScreen(custom);
    else
        delete custom;
}

static void startPlaybackWithGroup(const QString& recGroup = "")
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *pbb = new PlaybackBox(mainStack, "playbackbox");

    if (pbb->Create())
    {
        if (!recGroup.isEmpty())
            pbb->setInitialRecGroup(recGroup);

        mainStack->AddScreen(pbb);
    }
    else
    {
        delete pbb;
    }
}

static void startPlayback(void)
{
    startPlaybackWithGroup();
}

static void startPrevious(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *pl = new PrevRecordedList(mainStack);
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

static void startPreviousOld(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *pl = new ProgLister(mainStack);
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

static void startCustomEdit(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *custom = new CustomEdit(mainStack);

    if (custom->Create())
        mainStack->AddScreen(custom);
    else
        delete custom;
}

static void startManualSchedule(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *mansched= new ManualSchedule(mainStack);

    if (mansched->Create())
        mainStack->AddScreen(mansched);
    else
        delete mansched;
}

static bool isLiveTVAvailable(void)
{
    if (RemoteGetFreeRecorderCount() > 0)
        return true;

    QString msg = QCoreApplication::translate("(Common)", "All tuners are currently busy.");

    if (TV::ConfiguredTunerCards() < 1)
        msg = QCoreApplication::translate("(Common)", "There are no configured tuners.");

    ShowOkPopup(msg);
    return false;
}

static void startTVNormal(void)
{
    if (!isLiveTVAvailable())
        return;

    // Get the default channel keys (callsign(0) and channum(1)) and
    // use them to generate the ordered list of channels.
    QStringList keylist = gCoreContext->GetSettingOnHost(
        "DefaultChanKeys", gCoreContext->GetHostName()).split("[]:[]");
    while (keylist.size() < 2)
        keylist << "";
    uint dummy = 0;
    ChannelInfoList livetvchannels = ChannelUtil::LoadChannels(
        0,                      // startIndex
        0,                      // count
        dummy,                  // totalAvailable
        true,                   // ignoreHidden
        ChannelUtil::kChanOrderByLiveTV, // orderBy
        ChannelUtil::kChanGroupByChanid, // groupBy
        0,                      // sourceID
        0,                      // channelGroupID
        true,                   // liveTVOnly
        keylist[0],             // callsign
        keylist[1]);            // channum

    TV::StartTV(nullptr, kStartTVNoFlags, livetvchannels);
}

static void showStatus(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *statusbox = new StatusBox(mainStack);

    if (statusbox->Create())
        mainStack->AddScreen(statusbox);
    else
        delete statusbox;
}


static void standbyScreen(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *idlescreen = new IdleScreen(mainStack);

    if (idlescreen->Create())
        mainStack->AddScreen(idlescreen);
    else
        delete idlescreen;
}

static void RunVideoScreen(VideoDialog::DialogType type, bool fromJump = false)
{
    QString message = QCoreApplication::translate("(MythFrontendMain)",
                                      "Loading videos ...");

    MythScreenStack *popupStack =
            GetMythMainWindow()->GetStack("popup stack");

    auto *busyPopup = new MythUIBusyDialog(message, popupStack,
                                           "mythvideobusydialog");

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
                        " within %1ms").arg(VideoListDeathDelay::kDelayTimeMS.count()));
        }
    }

    VideoDialog::BrowseType browse = static_cast<VideoDialog::BrowseType>(
                         gCoreContext->GetNumSetting("mythvideo.db_group_type",
                                                 VideoDialog::BRS_FOLDER));

    if (!video_list)
        video_list = new VideoList;

    auto *mythvideo =
            new VideoDialog(mainStack, "mythvideo", video_list, type, browse);

    if (mythvideo->Create())
    {
        busyPopup->Close();
        mainStack->AddScreen(mythvideo);
    }
    else
    {
        busyPopup->Close();
    }
}

static void jumpScreenVideoManager() { RunVideoScreen(VideoDialog::DLG_MANAGER, true); }
static void jumpScreenVideoBrowser() { RunVideoScreen(VideoDialog::DLG_BROWSER, true); }
static void jumpScreenVideoTree()    { RunVideoScreen(VideoDialog::DLG_TREE, true);    }
static void jumpScreenVideoGallery() { RunVideoScreen(VideoDialog::DLG_GALLERY, true); }
static void jumpScreenVideoDefault() { RunVideoScreen(VideoDialog::DLG_DEFAULT, true); }

static void RunGallery()
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    auto *galleryView = new GalleryThumbView(mainStack, "galleryview");
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
                                         0, 0, "", 0min, "", "", true);

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
#ifdef Q_OS_DARWIN
            // Convert a BSD 'leaf' name into a raw device path
            QString filename = "dvd://dev/r";   // e.g. 'dvd://dev/rdisk2'
#elif defined(_WIN32)
            QString filename = "dvd:";          // e.g. 'dvd:E\\'
#else
            QString filename = "dvd:/";         // e.g. 'dvd://dev/sda'
#endif
            filename += dvd_device;

            command_string = "Internal";
            GetMythMainWindow()->HandleMedia(command_string, filename, "", "",
                                             "", "", 0, 0, "", 0min, "", "", true);
            GetMythUI()->RemoveCurrentLocation();

            return;
        }

        if (command_string.contains("%d"))
        {
            //
            //  Need to do device substitution
            //
            command_string = command_string.replace("%d", dvd_device);
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


    for (const auto *screen : std::as_const(screens))
    {
        if (qobject_cast<const GalleryThumbView*>(screen))
        {
            // Running gallery will receive this event later
            LOG(VB_MEDIA, LOG_INFO, "Main: Ignoring new gallery media - already running");
            return;
        }
    }

    if (gCoreContext->GetBoolSetting("GalleryAutoLoad", false))
    {
        LOG(VB_GUI, LOG_INFO, "Main: Autostarting Gallery for new media");
        GetMythMainWindow()->JumpTo(JUMP_GALLERY_DEFAULT);
    }
    else
    {
        LOG(VB_MEDIA, LOG_INFO, "Main: Ignoring new gallery media - autorun not set");
    }
}

static void TVMenuCallback([[maybe_unused]] void *data, QString &selection)
{
    QString sel = selection.toLower();

    if (sel.startsWith("settings ") || sel == "video_settings_general")
    {
        if (!g_settingsHelper)
            g_settingsHelper = new SettingsHelper;

        g_settingsHelper->RunProlog(sel);
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
    {
        startGuide();
    }
    else if (sel == "tv_manualschedule")
    {
        startManualSchedule();
    }
    else if (sel == "tv_custom_record")
    {
        startCustomEdit();
    }
    else if (sel == "tv_fix_conflicts")
    {
        startManaged();
    }
    else if (sel == "tv_manage_recording_rules")
    {
        startManageRecordingRules();
    }
    else if (sel == "tv_progfind")
    {
        startFinder();
    }
    else if (sel == "tv_search_title")
    {
        startSearchTitle();
    }
    else if (sel == "tv_search_keyword")
    {
        startSearchKeyword();
    }
    else if (sel == "tv_search_people")
    {
        startSearchPeople();
    }
    else if (sel == "tv_search_power")
    {
        startSearchPower();
    }
    else if (sel == "tv_search_stored")
    {
        startSearchStored();
    }
    else if (sel == "tv_search_channel")
    {
        startSearchChannel();
    }
    else if (sel == "tv_search_category")
    {
        startSearchCategory();
    }
    else if (sel == "tv_search_movie")
    {
        startSearchMovie();
    }
    else if (sel == "tv_search_new")
    {
        startSearchNew();
    }
    else if (sel == "tv_search_time")
    {
        startSearchTime();
    }
    else if (sel == "tv_previous")
    {
        startPrevious();
    }
    else if (sel == "tv_previous_old")
    {
        startPreviousOld();
    }
    else if (sel == "settings appearance")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        auto *ssd = new StandardSettingDialog(mainStack, "videogeneralsettings",
                                              new AppearanceSettings());

        if (ssd->Create())
        {
            mainStack->AddScreen(ssd);
        }
        else
        {
            delete ssd;
        }
    }
    else if (sel == "settings themechooser")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        auto *tp = new ThemeChooser(mainStack);

        if (tp->Create())
            mainStack->AddScreen(tp);
        else
            delete tp;
    }
    else if (sel == "settings setupwizard")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        auto *sw = new GeneralSetupWizard(mainStack, "setupwizard");

        if (sw->Create())
            mainStack->AddScreen(sw);
        else
            delete sw;
    }
    else if (sel == "settings grabbers")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        auto *gs = new GrabberSettings(mainStack, "grabbersettings");

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
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        auto *ssd = new StandardSettingDialog(mainStack, "playbackgroupsetting",
                                              new PlayGroupEditor());

        if (ssd->Create())
        {
            mainStack->AddScreen(ssd);
        }
        else
        {
            delete ssd;
        }
    }
    else if (sel == "settings general")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        auto *ssd = new StandardSettingDialog(mainStack, "videogeneralsettings",
                                              new GeneralSettings());

        if (ssd->Create())
        {
            mainStack->AddScreen(ssd);
        }
        else
        {
            delete ssd;
        }
    }
    else if (sel == "settings audiogeneral")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        StandardSettingDialog *ssd =
            new AudioConfigScreen(mainStack, "audiogeneralsettings",
                                  new AudioConfigSettings());

        if (ssd->Create())
        {
            mainStack->AddScreen(ssd);
        }
        else
        {
            delete ssd;
        }
    }
    else if (sel == "settings maingeneral")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        auto *ssd = new StandardSettingDialog(mainStack, "maingeneralsettings",
                                              new MainGeneralSettings());

        if (ssd->Create())
        {
            mainStack->AddScreen(ssd);
        }
        else
        {
            delete ssd;
        }
    }
    else if (sel == "settings playback")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        StandardSettingDialog *ssd =
            new PlaybackSettingsDialog(mainStack);

        if (ssd->Create())
        {
            mainStack->AddScreen(ssd);
        }
        else
        {
            delete ssd;
        }
    }
    else if (sel == "settings osd")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        auto *ssd = new StandardSettingDialog(mainStack, "osdsettings",
                                              new OSDSettings());

        if (ssd->Create())
        {
            mainStack->AddScreen(ssd);
        }
        else
        {
            delete ssd;
        }
    }
    else if (sel == "settings epg")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        auto *ssd = new StandardSettingDialog(mainStack, "epgsettings",
                                              new EPGSettings());

        if (ssd->Create())
        {
            mainStack->AddScreen(ssd);
        }
        else
        {
            delete ssd;
        }
    }
    else if (sel == "settings channelgroups")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        auto *ssd = new StandardSettingDialog(mainStack, "channelgroupssettings",
                                              new ChannelGroupsSetting());

        if (ssd->Create())
        {
            mainStack->AddScreen(ssd);
        }
        else
        {
            delete ssd;
        }
    }
    else if (sel == "settings generalrecpriorities")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        auto *ssd = new StandardSettingDialog(mainStack,
                                              "generalrecprioritiessettings",
                                              new GeneralRecPrioritiesSettings());

        if (ssd->Create())
        {
            mainStack->AddScreen(ssd);
        }
        else
        {
            delete ssd;
        }
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

        auto *msee = new MythSystemEventEditor(mainStack, "System Event Editor");

        if (msee->Create())
            mainStack->AddScreen(msee);
        else
            delete msee;
    }
    else if (sel == "video_settings_general")
    {
        RunSettingsCompletion::Create(gCoreContext->
                GetBoolSetting("VideoAggressivePC", false));
    }
    else if (sel == "video_settings_player")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

        auto *ps = new PlayerSettings(mainStack, "player settings");

        if (ps->Create())
            mainStack->AddScreen(ps);
        else
            delete ps;
    }
    else if (sel == "video_settings_metadata")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

        auto *ms = new MetadataSettings(mainStack, "metadata settings");

        if (ms->Create())
            mainStack->AddScreen(ms);
        else
            delete ms;
    }
    else if (sel == "video_settings_associations")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

        auto *fa = new FileAssocDialog(mainStack, "fa dialog");

        if (fa->Create())
            mainStack->AddScreen(fa);
    }
    else if (sel == "manager")
    {
        RunVideoScreen(VideoDialog::DLG_MANAGER);
    }
    else if (sel == "browser")
    {
        RunVideoScreen(VideoDialog::DLG_BROWSER);
    }
    else if (sel == "listing")
    {
        RunVideoScreen(VideoDialog::DLG_TREE);
    }
    else if (sel == "gallery")
    {
        RunVideoScreen(VideoDialog::DLG_GALLERY);
    }
    else if (sel == "disc_play")
    {
        playDisc();
    }
    else if (sel == "tv_status")
    {
        showStatus();
    }
    else if (sel == "exiting_app_prompt")
    {
        handleExit(true);
    }
    else if (sel == "exiting_app")
    {
        handleExit(false);
    }
    else if (sel == "standby_mode")
    {
        standbyScreen();
    }
    else if (sel == "exiting_menu")
    {
        //ignore
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Unknown menu action: " + selection);
    }

    if (sel.startsWith("settings ") || sel == "video_settings_general")
    {
        if (g_settingsHelper)
        {
            QObject::connect(GetMythMainWindow()->GetMainStack()->GetTopScreen(),
                             &MythScreenType::Exiting,
                             g_settingsHelper, &SettingsHelper::RunEpilog);
        }
    }
}

static void handleExit(bool prompt)
{
    if (prompt)
    {
        auto * prompter = new ExitPrompter();
        prompter->HandleExit();
    }
    else
    {
        QCoreApplication::quit();
    }
}

static bool RunMenu(const QString& themedir, const QString& themename)
{
    QByteArray tmp = themedir.toLocal8Bit();
    g_menu = new MythThemedMenu(QString(tmp.constData()), "mainmenu.xml",
                                GetMythMainWindow()->GetMainStack(), "mainmenu");

    if (g_menu->foundTheme())
    {
        LOG(VB_GENERAL, LOG_NOTICE, QString("Found mainmenu.xml for theme '%1'")
                .arg(themename));
        g_menu->setCallback(TVMenuCallback, gContext);
        GetMythMainWindow()->GetMainStack()->AddScreen(g_menu);
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, QString("Couldn't find mainmenu.xml for theme '%1'")
        .arg(themename));
    delete g_menu;
    g_menu = nullptr;
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
    //TODo Playback group not loaded?
    //TODo Channel group not loaded?
}

static int internal_play_media(const QString &mrl, const QString &plot,
                        const QString &title, const QString &subtitle,
                        const QString &director, int season, int episode,
                        const QString &inetref, std::chrono::minutes lenMins,
                        const QString &year,
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
        QString errorText = QCoreApplication::translate("(MythFrontendMain)",
            "Failed to open \n '%1' in %2 \n"
            "Check if the video exists")
            .arg(mrl.section('/', -1),
                 mrl.section('/', 0, -2));

        ShowOkPopup(errorText);
        return res;
    }

    auto *pginfo = new ProgramInfo(
        mrl, plot, title, QString(), subtitle, QString(),
        director, season, episode, inetref, lenMins,
        (year.toUInt()) ? year.toUInt() : 1900, id);

    pginfo->SetProgramInfoType(pginfo->DiscoverProgramInfoType());

    bool bookmarkPresent = false;
    bool lastPlayPresent = false;

    if (pginfo->IsVideoDVD())
    {
        auto *dvd = new MythDVDInfo(pginfo->GetPlaybackURL());
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
            ShowNotificationError(QCoreApplication::translate("(MythFrontendMain)",
                                                  "DVD Failure"),
                                                  sLocation,
                                                  dvd->GetLastError());
            delete dvd;
            delete pginfo;
            return res;
        }
        delete dvd;
    }
    else if (pginfo->IsVideoBD())
    {
        MythBDInfo bd(pginfo->GetPlaybackURL());
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
            ShowNotificationError(QCoreApplication::translate("(MythFrontendMain)",
                                                  "BD Failure"),
                                                  sLocation,
                                                  bd.GetLastError());
            delete pginfo;
            return res;
        }
    }
    else if (useBookmark && pginfo->IsVideo())
    {
        pginfo->SetIgnoreLastPlayPos(false);
        pginfo->SetIgnoreBookmark(false);
        bookmarkPresent = pginfo->QueryBookmark() > 0;
        lastPlayPresent = pginfo->QueryLastPlayPos() > 0;
    }

    if (useBookmark && (bookmarkPresent || lastPlayPresent))
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        auto *bookmarkdialog = new BookmarkDialog(pginfo, mainStack,
                                                  bookmarkPresent,
                                                  lastPlayPresent);
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
    auto *lmenu = qobject_cast<MythThemedMenuState *>
        (GetMythMainWindow()->GetMainStack()->GetTopScreen());
    if (lmenu)
        lmenu->m_buttonList->SetItemCurrent(0);
}

// If the theme specified in the DB is somehow broken, try a standard one:
//
static bool resetTheme(QString themedir, const QString &badtheme)
{
    QString themename = DEFAULT_UI_THEME;

    if (badtheme == DEFAULT_UI_THEME)
        themename = FALLBACK_UI_THEME;

    LOG(VB_GENERAL, LOG_WARNING, QString("Overriding broken theme '%1' with '%2'")
        .arg(badtheme, themename));

    gCoreContext->OverrideSettingForSession("Theme", themename);
    themedir = GetMythUI()->FindThemeDir(themename);

    MythTranslation::reload();
    gCoreContext->ReInitLocale();
    GetMythMainWindow()->Init();

    return RunMenu(themedir, themename);
}

static int reloadTheme(void)
{
#ifdef Q_OS_ANDROID
    // jni code to launch the application again
    // reinitializing the main windows causes a segfault
    // with android

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    auto activity = QtAndroid::androidActivity();
#else
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
#endif
    auto packageManager = activity.callObjectMethod
        (   "getPackageManager",
            "()Landroid/content/pm/PackageManager;"  );

    auto activityIntent = packageManager.callObjectMethod
        (   "getLaunchIntentForPackage",
            "(Ljava/lang/String;)Landroid/content/Intent;",
            activity.callObjectMethod("getPackageName",
            "()Ljava/lang/String;").object()  );

    auto pendingIntent = QAndroidJniObject::callStaticObjectMethod
        (   "android/app/PendingIntent",
            "getActivity",
            "(Landroid/content/Context;ILandroid/content/Intent;I)Landroid/app/PendingIntent;",
            activity.object(),
            0,
            activityIntent.object(),
            QAndroidJniObject::getStaticField<jint>("android/content/Intent",
            "FLAG_ACTIVITY_CLEAR_TOP")  );

    auto alarmManager = activity.callObjectMethod
        (   "getSystemService",
            "(Ljava/lang/String;)Ljava/lang/Object;",
            QAndroidJniObject::getStaticObjectField("android/content/Context",
            "ALARM_SERVICE",
            "Ljava/lang/String;").object()  );

    alarmManager.callMethod<void>
        (   "set",
            "(IJLandroid/app/PendingIntent;)V",
            QAndroidJniObject::getStaticField<jint>("android/app/AlarmManager", "RTC"),
            jlong(QDateTime::currentMSecsSinceEpoch() + 100),
            pendingIntent.object()  );

    qApp->quit();
    // QString title = QObject::tr("Your change will take effect the next time "
    //                     "mythfrontend is started.");
    //     MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");    MythConfirmationDialog *okPopup =
    //         new MythConfirmationDialog(popupStack, title, false);
    // if (okPopup->Create())
    //     popupStack->AddScreen(okPopup);
    return 0;
#else
    GetMythUI()->InitThemeHelper();
    QString themename = gCoreContext->GetSetting("Theme", DEFAULT_UI_THEME);
    QString themedir  = GetMythUI()->FindThemeDir(themename);
    if (themedir.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Couldn't find theme '%1'").arg(themename));
        return GENERIC_EXIT_NO_THEME;
    }

    gCoreContext->ReInitLocale();
    MythTranslation::reload();
    GetMythMainWindow()->SetEffectsEnabled(false);
    if (g_menu)
        g_menu->Close();
    GetMythMainWindow()->Init();
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
#endif
}

static void reloadTheme_void(void)
{
    int err = reloadTheme();
    if (err)
        exit(err);
}

static void setDebugShowBorders(void)
{
    MythMainWindow* window = GetMythMainWindow();
    MythPainter* painter = window->GetPainter();
    painter->SetDebugMode(!painter->ShowBorders(), painter->ShowTypeNames());
    if (window->GetMainStack()->GetTopScreen())
        window->GetMainStack()->GetTopScreen()->SetRedraw();
}

static void setDebugShowNames(void)
{
    MythMainWindow* window = GetMythMainWindow();
    MythPainter* painter = window->GetPainter();
    painter->SetDebugMode(painter->ShowBorders(), !painter->ShowTypeNames());
    if (window->GetMainStack()->GetTopScreen())
        window->GetMainStack()->GetTopScreen()->SetRedraw();
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
         "Show Incremental Search Dialog"), "Ctrl+S,Search");
     REG_KEY("Video","DOWNLOADDATA", QT_TRANSLATE_NOOP("MythControls",
         "Download metadata for current item"), "W");
     REG_KEY("Video","ITEMDETAIL", QT_TRANSLATE_NOOP("MythControls",
         "Display Item Detail Popup"), "");

     // Gallery keybindings
     REG_KEY("Images", "PLAY", QT_TRANSLATE_NOOP("MythControls",
         "Start/Stop Slideshow"), "P,Media Play");
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
         "Zoom image out"), "7,<,Ctrl+B,Media Rewind");
     REG_KEY("Images", "ZOOMIN", QT_TRANSLATE_NOOP("MythControls",
         "Zoom image in"), "9,>,Ctrl+F,Media Fast Forward");
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
    MythMainWindow* mainwindow = GetMythMainWindow();
    if (mainwindow)
        mainwindow->ClearKeyContext("Video");
    InitKeys();
    if (mainwindow)
        mainwindow->ReloadKeys();
}

static void SetFuncPtrs(void)
{
    TV::SetFuncPtr("playbackbox",   (void *)PlaybackBox::RunPlaybackBox);
    TV::SetFuncPtr("viewscheduled", (void *)ViewScheduled::RunViewScheduled);
    TV::SetFuncPtr("programguide",  (void *)GuideGrid::RunProgramGuide);
    TV::SetFuncPtr("programlist",   (void *)ProgLister::RunProgramList);
    TV::SetFuncPtr("scheduleeditor", (void *)ScheduleEditor::RunScheduleEditor);
    TV::SetFuncPtr("programfinder",  (void *)RunProgramFinder);
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
        "MythDVD DVD Media Handler"), "", handleDVDMedia,
        MEDIATYPE_DVD, QString());
    REG_MEDIA_HANDLER(QT_TRANSLATE_NOOP("MythControls",
        "MythImage Media Handler 1/2"), "", handleGalleryMedia,
        MEDIATYPE_DATA | MEDIATYPE_MIXED, QString());

    QStringList extensions(ImageAdapterBase::SupportedImages()
                           + ImageAdapterBase::SupportedVideos());

    REG_MEDIA_HANDLER(QT_TRANSLATE_NOOP("MythControls",
        "MythImage Media Handler 2/2"), "", handleGalleryMedia,
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
    if( MythCoreContext::BackendIsRunning() )
    {
        QDateTime startupTime = QDateTime();

        if( gCoreContext->IsMasterHost() )
        {
            QString s = gCoreContext->GetSetting("MythShutdownWakeupTime", "");
            if (!s.isEmpty())
                startupTime = MythDate::fromString(s);

            // if we don't have a valid startup time assume we were started manually
            if (startupTime.isValid())
            {
                auto startupSecs = gCoreContext->GetDurSetting<std::chrono::seconds>("StartupSecsBeforeRecording");
                startupSecs = std::max(startupSecs, 15 * 60s);
                // If we started within 'StartupSecsBeforeRecording' OR 15 minutes
                // of the saved wakeup time assume we either started automatically
                // to record, to obtain guide data or or for a
                // daily wakeup/shutdown period
                if (abs(MythDate::secsInPast(startupTime)) < startupSecs)
                {
                    LOG(VB_GENERAL, LOG_INFO,
                        "Close to auto-start time, AUTO-Startup assumed");

                    QString str = gCoreContext->GetSetting("MythFillSuggestedRunTime");
                    QDateTime guideRunTime = MythDate::fromString(str);
                    if (MythDate::secsInPast(guideRunTime) < startupSecs)
                    {
                        LOG(VB_GENERAL, LOG_INFO,
                            "Close to MythFillDB suggested run time, AUTO-Startup to fetch guide data?");
                    }
                    autoStart = true;
                }
                else
                {
                    LOG(VB_GENERAL, LOG_DEBUG,
                        "NOT close to auto-start time, USER-initiated startup assumed");
                }
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
                bool        bConflicts = false;
                QDateTime   nextRecordingStart;

                if (LoadFromScheduler(progList, bConflicts))
                {
                    // Find the first recording to be recorded
                    // on this machine
                    QString hostname = gCoreContext->GetHostName();
                    for (auto *prog : progList)
                    {
                        if ((prog->GetRecordingStatus() == RecStatus::WillRecord ||
                             prog->GetRecordingStatus() == RecStatus::Pending) &&
                            (prog->GetHostname() == hostname) &&
                            (nextRecordingStart.isNull() ||
                             nextRecordingStart > prog->GetRecordingStartTime()))
                        {
                            nextRecordingStart = prog->GetRecordingStartTime();
                        }
                    }

                    if (!nextRecordingStart.isNull() &&
                        (abs(MythDate::secsInPast(nextRecordingStart)) < 4min))
                    {
                        LOG(VB_GENERAL, LOG_INFO,
                            "Close to start time, AUTO-Startup assumed");

                        // If we started within 4 minutes of the next recording,
                        // we almost certainly started automatically.
                        autoStart = true;
                    }
                    else
                    {
                        LOG(VB_GENERAL, LOG_DEBUG,
                            "NOT close to auto-start time, USER-initiated startup assumed");
                    }

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


Q_DECL_EXPORT int main(int argc, char **argv)
{
    bool bPromptForBackend    = false;
    bool bBypassAutoDiscovery = false;

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
        MythFrontendCommandLineParser::PrintVersion();
        return GENERIC_EXIT_OK;
    }

    MythDisplay::ConfigureQtGUI(1, cmdline);
    QApplication::setSetuidAllowed(true);
    QApplication a(argc, argv);
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHFRONTEND);
    CleanupGuard callCleanup(cleanup);

#ifdef Q_OS_DARWIN
    QString path = QCoreApplication::applicationDirPath();
    setenv("PYTHONPATH",
           QString("%1/../Resources/lib/%2/site-packages:%3")
           .arg(path)
           .arg(QFileInfo(PYTHON_EXE).fileName())
           .arg(QProcessEnvironment::systemEnvironment().value("PYTHONPATH"))
           .toUtf8().constData(), 1);
#endif

#ifndef _WIN32
    SignalHandler::Init();
    SignalHandler::SetHandler(SIGUSR1, handleSIGUSR1);
    SignalHandler::SetHandler(SIGUSR2, handleSIGUSR2);
#endif

#if defined(Q_OS_ANDROID)
    auto config = QSslConfiguration::defaultConfiguration();
    config.setCaCertificates(QSslConfiguration::systemCaCertificates());
    QSslConfiguration::setDefaultConfiguration(config);
#endif

    int retval = cmdline.ConfigureLogging();
    if (retval != GENERIC_EXIT_OK)
        return retval;

    bool ResetSettings = false;

    if (cmdline.toBool("prompt"))
        bPromptForBackend = true;
    if (cmdline.toBool("noautodiscovery"))
        bBypassAutoDiscovery = true;

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        std::cerr << "Unable to ignore SIGPIPE\n";

    if (!cmdline.toString("geometry").isEmpty())
        MythMainWindow::ParseGeometryOverride(cmdline.toString("geometry"));

    fe_sd_notify("STATUS=Connecting to database.");
    gContext = new MythContext(MYTH_BINARY_VERSION, true);
    gCoreContext->SetAsFrontend(true);

    cmdline.ApplySettingsOverride();
    if (!gContext->Init(true, bPromptForBackend, bBypassAutoDiscovery))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to init MythContext, exiting.");
        gCoreContext->SetExiting(true);
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

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
        fe_sd_notify("STATUS=Creating UPnP media renderer");
        g_pUPnp  = new MediaRenderer();
        if (!g_pUPnp->isInitialized())
        {
            delete g_pUPnp;
            g_pUPnp = nullptr;
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
        gCoreContext->GetDB()->ClearSettingOnHost("Language", nullptr);
        gCoreContext->GetDB()->ClearSetting("Country");
        gCoreContext->GetDB()->ClearSettingOnHost("Country", nullptr);

        LOG(VB_GENERAL, LOG_NOTICE, "Appearance settings and language have "
                                    "been reset to defaults. You will need to "
                                    "restart the frontend.");
        gContext-> saveSettingsCache();
        return GENERIC_EXIT_OK;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    int maxImageSize = gCoreContext->GetNumSetting("ImageMaximumSize", -1);
    if (maxImageSize >=0)
        QImageReader::setAllocationLimit(maxImageSize);
#endif
    LOG(VB_GENERAL, LOG_DEBUG,
        QString("Built against zlib %1, linked against %2.")
        .arg(ZLIB_VERSION, zlibVersion()));
    QList<QByteArray> formats = QImageReader::supportedImageFormats();
    QString format_str = formats.takeFirst();
    for (const auto& format : std::as_const(formats))
        format_str += ", " + format;
    LOG(VB_GENERAL, LOG_DEBUG, QString("Supported image formats: %1").arg(format_str));

    QCoreApplication::setSetuidAllowed(true);

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
        fe_sd_notify("STATUS=Registering frontend with bonjour");
        QByteArray dummy;
        int port = gCoreContext->GetNumSetting("UPnP/MythFrontend/ServicePort", 6547);
        // frontend upnp server is now ServicePort + 4 (default 6551)
        port += 4;
        QByteArray name("Mythfrontend on ");
        name.append(gCoreContext->GetHostName().toUtf8());
        bonjour->Register(port, "_mythfrontend._tcp",
                                 name, dummy);
    }
#endif

    fe_sd_notify("STATUS=Initializing LCD");
    LCD::SetupLCD();
    if (LCD *lcd = LCD::Get())
        lcd->setupLEDs(RemoteGetRecordingMask);

    fe_sd_notify("STATUS=Loading translation");
    MythTranslation::load("mythfrontend");

    fe_sd_notify("STATUS=Loading themes");
    QString themename = gCoreContext->GetSetting("Theme", DEFAULT_UI_THEME);

    QString themedir = GetMythUI()->FindThemeDir(themename);
    if (themedir.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Couldn't find theme '%1'")
                .arg(themename));
        return GENERIC_EXIT_NO_THEME;
    }

    themename = gCoreContext->GetSetting("Theme", DEFAULT_UI_THEME);
    themedir = GetMythUI()->FindThemeDir(themename);
    if (themedir.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Couldn't find theme '%1'")
                .arg(themename));
        return GENERIC_EXIT_NO_THEME;
    }

    auto * mainWindow = GetMythMainWindow();

    // Force an update of our hardware decoder/render support once the window is
    // ready and we have a render device (and after each window re-initialisation
    // when we may have a new render device). This also ensures the support checks
    // are done immediately and are not reliant on semi-random settings initialisation.
    QObject::connect(mainWindow, &MythMainWindow::SignalWindowReady,
                     []() { MythVideoProfile::InitStatics(true); } );

    mainWindow->Init(false);
    mainWindow->setWindowTitle(QCoreApplication::translate("(MythFrontendMain)",
                                               "MythTV Frontend",
                                               "Main window title"));

#ifdef USING_AIRPLAY
    if (gCoreContext->GetBoolSetting("AirPlayEnabled", true))
    {
        fe_sd_notify("STATUS=Initializing AirPlay");
        MythRAOPDevice::Create();
        if (!gCoreContext->GetBoolSetting("AirPlayAudioOnly", false))
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

    if (!UpgradeTVDatabaseSchema(false, false, true))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Couldn't upgrade database to new schema, exiting.");
        return GENERIC_EXIT_DB_OUTOFDATE;
    }

    WriteDefaults();

    // Refresh Global/Main Menu keys after DB update in case there was no DB
    // when they were written originally
    mainWindow->ReloadKeys();

    fe_sd_notify("STATUS=Initializing jump points");
    InitJumpPoints();
    InitKeys();
    TV::InitKeys();
    SetFuncPtrs();

    internal_media_init();

    CleanupMyOldInUsePrograms();

    setHttpProxy();

    fe_sd_notify("STATUS=Initializing plugins");
    g_pmanager = new MythPluginManager();
    gCoreContext->SetPluginManager(g_pmanager);

    fe_sd_notify("STATUS=Initializing media monitor");
    MediaMonitor *mon = MediaMonitor::GetMediaMonitor();
    if (mon)
    {
        mon->StartMonitoring();
        mainWindow->installEventFilter(mon);
    }

    fe_sd_notify("STATUS=Initializing network control");
    NetworkControl *networkControl = nullptr;
    if (gCoreContext->GetBoolSetting("NetworkControlEnabled", false))
    {
        int port = gCoreContext->GetNumSetting("NetworkControlPort", 6546);
        networkControl = new NetworkControl();
        if (!networkControl->listen(port))
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("NetworkControl failed to bind to port %1.")
                   .arg(port));
        }
    }

#ifdef Q_OS_DARWIN
    GetMythMainWindow()->SetEffectsEnabled(false);
    GetMythMainWindow()->Init();
    GetMythMainWindow()->SetEffectsEnabled(true);
    gLoaded = true;
#endif
    if (!RunMenu(themedir, themename) && !resetTheme(themedir, themename))
    {
        return GENERIC_EXIT_NO_THEME;
    }
    fe_sd_notify("STATUS=Loading theme updates");
    std::unique_ptr<ThemeUpdateChecker> themeUpdateChecker;
    if (gCoreContext->GetBoolSetting("ThemeUpdateNofications", true))
        themeUpdateChecker = std::make_unique<ThemeUpdateChecker>();

    MythSystemEventHandler sysEventHandler {};

    BackendConnectionManager bcm;

    PreviewGeneratorQueue::CreatePreviewGeneratorQueue(
        PreviewGenerator::kRemote, 50, 60s);

    fe_sd_notify("STATUS=Creating housekeeper");
    auto *housekeeping = new HouseKeeper();
#ifdef __linux__
 #ifdef CONFIG_BINDINGS_PYTHON
    housekeeping->RegisterTask(new HardwareProfileTask());
 #endif
#endif
    housekeeping->Start();


    if (cmdline.toBool("runplugin"))
    {
        QStringList plugins = g_pmanager->EnumeratePlugins();

        if (plugins.contains(cmdline.toString("runplugin")))
            g_pmanager->run_plugin(cmdline.toString("runplugin"));
        else if (plugins.contains("myth" + cmdline.toString("runplugin")))
            g_pmanager->run_plugin("myth" + cmdline.toString("runplugin"));
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

    // Provide systemd ready notification (for type=notify units)
    fe_sd_notify("STATUS=");
    fe_sd_notify("READY=1");


    int ret = 0;
    {
        MythHTTPInstance::Addservices({{ FRONTEND_SERVICE, &MythHTTPService::Create<MythFrontendService> }});

        // Send all unknown requests into the web app. make bookmarks and direct access work.
        auto spa_index = [](auto && PH1) { return MythHTTPRewrite::RewriteToSPA(std::forward<decltype(PH1)>(PH1), "mythfrontend.html"); };
        MythHTTPInstance::AddErrorPageHandler({ "=404", spa_index });

        auto root = [](auto && PH1) { return MythHTTPRoot::RedirectRoot(std::forward<decltype(PH1)>(PH1), "mythfrontend.html"); };
        MythHTTPScopedInstance webserver({{ "/", root}});
        ret = QCoreApplication::exec();
    }

    fe_sd_notify("STOPPING=1\nSTATUS=Exiting");
    if (ret==0)
        gContext-> saveSettingsCache();

    DestroyMythUI();
    PreviewGeneratorQueue::TeardownPreviewGeneratorQueue();

    delete housekeeping;

    g_pmanager->DestroyAllPlugins();

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
    GetMythMainWindow()->RestartInputHandlers();
}

/*
include Qt MOC output for Q_OBJECT class defined in this file;
filenames must match.
*/
#include "mythfrontend.moc"
/* vim: set expandtab tabstop=4 shiftwidth=4: */
