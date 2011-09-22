#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <cerrno>

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

#include "previewgeneratorqueue.h"
#include "mythconfig.h"
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
#include "profilegroup.h"
#include "playgroup.h"
#include "networkcontrol.h"
#include "dvdringbuffer.h"
#include "scheduledrecording.h"
#include "mythsystemevent.h"
#include "hardwareprofile.h"

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
#include "lcddevice.h"
#include "langsettings.h"
#include "mythtranslation.h"
#include "commandlineparser.h"
#include "channelgroupsettings.h"

#include "myththemedmenu.h"
#include "mediarenderer.h"
#include "mythmainwindow.h"
#include "screenwizard.h"
#include "mythcontrols.h"
#include "mythuihelper.h"
#include "mythdirs.h"
#include "mythdb.h"
#include "backendconnectionmanager.h"
#include "themechooser.h"
#include "mythversion.h"
#include "taskqueue.h"

// Video
#include "cleanup.h"
#include "globals.h"
#include "videodlg.h"
#include "videoglobalsettings.h"
#include "videofileassoc.h"
#include "videoplayersettings.h"
#include "videometadatasettings.h"
#include "videolist.h"


static ExitPrompter   *exitPopup = NULL;
static MythThemedMenu *menu;

static QString         logfile;
static MediaRenderer  *g_pUPnp   = NULL;
static MythPluginManager *pmanager = NULL;

static void handleExit(bool prompt);
static void resetAllKeys(void);

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
        RunSettingsCompletion(bool check)
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
                        QObject::tr("Aggressive Parental Controls Warning: "
                                "invalid password. An attempt to enter a "
                                "MythVideo settings screen was prevented."));
            }

            deleteLater();
        }

      public:
        ParentalLevelChangeChecker m_plcc;
    };

    class BookmarkDialog : MythScreenType
    {
      public:
        BookmarkDialog(ProgramInfo *pginfo, MythScreenStack *parent) :
                MythScreenType(parent, "bookmarkdialog"),
                pgi(pginfo)
        {
        }

        bool Create()
        {
            QString msg = QObject::tr("DVD/Video contains a bookmark");
            QString btn0msg = QObject::tr("Play from bookmark");
            QString btn1msg = QObject::tr("Play from beginning");

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
        delete exitPopup;
        exitPopup = NULL;

        AudioOutput::Cleanup();

        if (g_pUPnp)
        {
            // This takes a few seconds, so inform the user:
            LOG(VB_GENERAL, LOG_INFO, "Deleting UPnP client...");
            delete g_pUPnp;
            g_pUPnp = NULL;
        }

        if (pmanager)
        {
            delete pmanager;
            pmanager = NULL;
        }

        delete gContext;
        gContext = NULL;

        delete qApp;

#ifndef _MSC_VER
        signal(SIGUSR1, SIG_DFL);
#endif
    }

    class CleanupGuard
    {
      public:
        typedef void (*CleanupFunc)();

      public:
        CleanupGuard(CleanupFunc cleanFunction) :
            m_cleanFunction(cleanFunction) {}

        ~CleanupGuard()
        {
            m_cleanFunction();
        }

      private:
        CleanupFunc m_cleanFunction;
    };
}

static void startAppearWiz(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    ScreenWizard *screenwizard = new ScreenWizard(mainStack,
                                                        "screenwizard");

    if (screenwizard->Create())
        mainStack->AddScreen(screenwizard);
    else
        delete screenwizard;
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
    channum = (channum.isEmpty()) ? "3" : channum;
    GuideGrid::RunProgramGuide(chanid, channum, NULL, false, true, -2);
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

static void startProgramRecPriorities(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    ProgramRecPriority *progRecPrior = new ProgramRecPriority(mainStack,
                                                        "ProgramRecPriority");

    if (progRecPrior->Create())
        mainStack->AddScreen(progRecPrior);
    else
        delete progRecPrior;
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
        mainStack, "playbackbox", PlaybackBox::kPlayBox);

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

static void startDelete(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    PlaybackBox *pbb = new PlaybackBox(
        mainStack, "deletebox", PlaybackBox::kDeleteBox);

    if (pbb->Create())
        mainStack->AddScreen(pbb);
    else
        delete pbb;
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

    QString msg = QObject::tr("All tuners are currently busy.");
    if (TV::ConfiguredTunerCards() < 1)
        msg = QObject::tr("There are no configured tuners.");

    ShowOkPopup(msg);
    return false;
}

static void startTVInGuide(void)
{
    if (isLiveTVAvailable())
        TV::StartTV(NULL, kStartTVInGuide);
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

static void RunVideoScreen(VideoDialog::DialogType type, bool fromJump = false)
{
    QString message = QObject::tr("Loading videos ...");

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

QString gDVDdevice;

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

    if (bdtest.exists())
        isBD = true;

    if (isBD)
    {
        GetMythUI()->AddCurrentLocation("playdisc");

        QString filename = QString("bd:/%1/").arg(bluray_mountpoint);

        GetMythMainWindow()->HandleMedia("Internal", filename);

        GetMythUI()->RemoveCurrentLocation();
    }
    else
    {
        QString dvd_device = gDVDdevice;

        if (dvd_device.isEmpty())
            dvd_device = MediaMonitor::defaultDVDdevice();

        if (dvd_device.isEmpty())
            return;  // User cancelled in the Popup

        GetMythUI()->AddCurrentLocation("playdisc");

        if ((command_string.indexOf("internal", 0, Qt::CaseInsensitive) > -1) ||
            (command_string.length() < 1))
        {
#ifdef Q_OS_MAC
            // Convert a BSD 'leaf' name into a raw device path
            QString filename = "dvd://dev/r";   // e.g. 'dvd://dev/rdisk2'
#elif USING_MINGW
            QString filename = "dvd:";          // e.g. 'dvd:E\\'
#else
            QString filename = "dvd:/";         // e.g. 'dvd://dev/sda'
#endif
            filename += dvd_device;

            command_string = "Internal";
            GetMythMainWindow()->HandleMedia(command_string, filename);
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
            sendPlaybackStart();
            myth_system(command_string);
            sendPlaybackEnd();
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

    QString newDevice = dvd->getDevicePath();

    // Device insertion. Store it for later use
    if (dvd->isUsable())
        if (gDVDdevice.length() && gDVDdevice != newDevice)
        {
            // Multiple DVD devices. Clear the old one so the user has to
            // select a disk to play (in MediaMonitor::defaultDVDdevice())

            LOG(VB_MEDIA, LOG_INFO,
                "MythVideo: Multiple DVD drives? Forgetting " + gDVDdevice);
            gDVDdevice.clear();
        }
        else
        {
            gDVDdevice = newDevice;
            LOG(VB_MEDIA, LOG_INFO,
                "MythVideo: Storing DVD device " + gDVDdevice);
        }
    else
    {
        // Ejected/unmounted/error.

        if (gDVDdevice.length() && gDVDdevice == newDevice)
        {
            LOG(VB_MEDIA, LOG_INFO,
                "MythVideo: Forgetting existing DVD " + gDVDdevice);
            gDVDdevice.clear();
        }

        return;
    }

    switch (gCoreContext->GetNumSetting("DVDOnInsertDVD", 1))
    {
        case 0 : // Do nothing
            break;
        case 1 : // Display menu (mythdvd)*/
            GetMythMainWindow()->JumpTo("Main Menu");
            break;
        case 2 : // play DVD or Blu-ray
            GetMythMainWindow()->JumpTo("Main Menu");
            playDisc();
            break;
        default:
            LOG(VB_GENERAL, LOG_ERR,
                "mythdvd main.o: handleMedia() does not know what to do");
    }
}

static void TVMenuCallback(void *data, QString &selection)
{
    (void)data;
    QString sel = selection.toLower();

    if (sel.left(9) == "settings ")
    {
        GetMythUI()->AddCurrentLocation("Setup");
        gCoreContext->ActivateSettingsCache(false);
        GetMythMainWindow()->HidePainterWindow();
    }

    if (sel == "tv_watch_live")
        startTVNormal();
    else if (sel == "tv_watch_live_epg")
        startTVInGuide();
    else if (sel.left(18) == "tv_watch_recording")
    {
        // use selection here because its case is untouched
        if ((selection.length() > 19) && (selection.mid(18, 1) == " "))
            startPlaybackWithGroup(selection.mid(19));
        else
            startPlayback();
    }
    else if (sel == "tv_schedule")
        startGuide();
    else if (sel == "tv_delete")
        startDelete();
    else if (sel == "tv_manualschedule")
        startManualSchedule();
    else if (sel == "tv_custom_record")
        startCustomEdit();
    else if (sel == "tv_fix_conflicts")
        startManaged();
    else if (sel == "tv_set_recpriorities")
        startProgramRecPriorities();
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
    else if (sel == "settings recording")
    {
        ProfileGroupEditor editor;
        editor.exec();
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
    else
        LOG(VB_GENERAL, LOG_ERR, "Unknown menu action: " + selection);

    if (sel.left(9) == "settings " || sel == "video_settings_general")
    {
        GetMythUI()->RemoveCurrentLocation();

        gCoreContext->ActivateSettingsCache(true);
        RemoteSendMessage("CLEAR_SETTINGS_CACHE");

        if (sel == "settings general" ||
            sel == "settings generalrecpriorities")
            ScheduledRecording::signalChange(0);
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
                        const QString &inetref, int lenMins, const QString &year)
{
    int res = -1;

    QFile checkFile(mrl);
    if ((!checkFile.exists() && !mrl.startsWith("dvd:")
         && !mrl.startsWith("bd:")
         && !mrl.startsWith("myth:")
         && !mrl.startsWith("http://")))
    {
        QString errorText = QObject::tr("Failed to open \n '%1' in %2 \n"
                                        "Check if the video exists")
                                        .arg(mrl.section('/', -1))
                                        .arg(mrl.section('/', 0, -2));
        ShowOkPopup(errorText);
        return res;
    }

    ProgramInfo *pginfo = new ProgramInfo(
        mrl, plot, title, subtitle, director, season, episode,
        inetref, lenMins, (year.toUInt()) ? year.toUInt() : 1900);

    pginfo->SetProgramInfoType(pginfo->DiscoverProgramInfoType());

    int64_t pos = 0;

    if (pginfo->IsVideoDVD())
    {
        DVDInfo *dvd = new DVDInfo(pginfo->GetPlaybackURL());
        if (dvd && dvd->IsValid())
        {
            QString name;
            QString serialid;
            if (dvd->GetNameAndSerialNum(name, serialid))
            {
                QStringList fields = pginfo->QueryDVDBookmark(serialid);
                if (!fields.empty())
                {
                    QStringList::Iterator it = fields.begin();
                    pos = (int64_t)((*++it).toLongLong() & 0xffffffffLL);
                }
            }
        }
        else
        {
            if (dvd)
                delete dvd;
            delete pginfo;
            return res;
        }
        delete dvd;
    }
    else if (pginfo->IsVideo())
        pos = pginfo->QueryBookmark();

    if (pos > 0)
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
        TV::StartTV(pginfo, kStartTVNoFlags);

        res = 0;

        delete pginfo;
    }

    return res;
}

static void gotoMainMenu(void)
{
}

// If the theme specified in the DB is somehow broken, try a standard one:
//
static bool resetTheme(QString themedir, const QString badtheme)
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
    GetMythUI()->LoadQtConfig();
    GetMythMainWindow()->Init();

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

    MythTranslation::reload();

    GetMythMainWindow()->SetEffectsEnabled(false);

    GetMythUI()->LoadQtConfig();

    menu->Close();
    GetMythMainWindow()->Init();

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
     REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Program Recording "
         "Priorities"), "", "", startProgramRecPriorities);
     REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Manage Recording Rules"),
         "", "", startManageRecordingRules);
     REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Channel Recording "
         "Priorities"), "", "", startChannelRecPriorities);
     REG_JUMPLOC(QT_TRANSLATE_NOOP("MythControls", "TV Recording Playback"),
         "", "", startPlayback, "JUMPREC");
     REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "TV Recording Deletion"),
         "", "", startDelete);
     REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Live TV"),
         "", "", startTVNormal);
     REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Live TV In Guide"),
         "", "", startTVInGuide);
     REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Status Screen"),
         "", "", showStatus);
     REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Previously Recorded"),
         "", "", startPrevious);

     // Video

     REG_JUMP(JUMP_VIDEO_DEFAULT, QT_TRANSLATE_NOOP("MythControls",
         "The Video default view"), "", jumpScreenVideoDefault);
     REG_JUMP(JUMP_VIDEO_MANAGER, QT_TRANSLATE_NOOP("MythControls",
         "The Video video manager"), "", jumpScreenVideoManager);
     REG_JUMP(JUMP_VIDEO_BROWSER, QT_TRANSLATE_NOOP("MythControls",
         "The Video video browser"), "", jumpScreenVideoBrowser);
     REG_JUMP(JUMP_VIDEO_TREE, QT_TRANSLATE_NOOP("MythControls",
         "The Video video listings"), "", jumpScreenVideoTree);
     REG_JUMP(JUMP_VIDEO_GALLERY, QT_TRANSLATE_NOOP("MythControls",
         "The Video video gallery"), "", jumpScreenVideoGallery);
     REG_JUMP("Play Disc", QT_TRANSLATE_NOOP("MythControls",
         "Play an Optical Disc"), "", playDisc);

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
     REG_KEY("Video","BROWSE", QT_TRANSLATE_NOOP("MythControls",
         "Change browsable in video manager"), "B");
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
     REG_KEY("Video","HOME", QT_TRANSLATE_NOOP("MythControls",
         "Go to the first video"), "Home");
     REG_KEY("Video","END", QT_TRANSLATE_NOOP("MythControls",
         "Go to the last video"), "End");
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


static void signal_USR1_handler(int){
      LOG(VB_GENERAL, LOG_NOTICE, "SIGUSR1 received, reloading theme");
      RemoteSendMessage("CLEAR_SETTINGS_CACHE");
      gCoreContext->ActivateSettingsCache(false);
      GetMythMainWindow()->JumpTo("Reload Theme");
      gCoreContext->ActivateSettingsCache(true);
}

static void signal_USR2_handler(int)
{
    LOG(VB_GENERAL, LOG_NOTICE, "SIGUSR2 received, restart LIRC handler");
    GetMythMainWindow()->StartLIRC();
}


static int internal_media_init()
{
    REG_MEDIAPLAYER("Internal", QT_TRANSLATE_NOOP("MythControls",
        "MythTV's native media player."), internal_play_media);
    REG_MEDIA_HANDLER(QT_TRANSLATE_NOOP("MythControls",
        "MythDVD DVD Media Handler"), "", "", handleDVDMedia,
        MEDIATYPE_DVD, QString::null);
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

int main(int argc, char **argv)
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
        cmdline.PrintVersion();
        return GENERIC_EXIT_OK;
    }

    CleanupGuard callCleanup(cleanup);

#ifdef Q_WS_MACX
    // Without this, we can't set focus to any of the CheckBoxSetting, and most
    // of the MythPushButton widgets, and they don't use the themed background.
    QApplication::setDesktopSettingsAware(false);
#endif
    new QApplication(argc, argv);
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHFRONTEND);

    QString pluginname;

    QFileInfo finfo(qApp->argv()[0]);
    QString binname = finfo.baseName();

    int retval;
    if ((retval = cmdline.ConfigureLogging()) != GENERIC_EXIT_OK)
        return retval;

    bool ResetSettings = false;

    if (binname.toLower() != "mythfrontend")
        pluginname = binname;

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

    if (!cmdline.toBool("noupnp"))
    {
        g_pUPnp  = new MediaRenderer();
        if (!g_pUPnp->initialized())
        {
            delete g_pUPnp;
            g_pUPnp = NULL;
        }
    }

    if (!gContext->Init(true, bPromptForBackend, bBypassAutoDiscovery))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to init MythContext, exiting.");
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

    if (cmdline.GetArgs().size() >= 1)
        pluginname = cmdline.GetArgs()[0];

    QString fileprefix = GetConfDir();

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    if (ResetSettings)
    {
        AppearanceSettings as;
        as.Save();

        gCoreContext->SaveSetting("Theme", DEFAULT_UI_THEME);
        gCoreContext->SaveSetting("Language", "");
        gCoreContext->SaveSetting("Country", "");

        return GENERIC_EXIT_OK;
    }

    setuid(getuid());

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
    mainWindow->Init();
    mainWindow->setWindowTitle(QObject::tr("MythTV Frontend"));

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
    gContext->SetPluginManager(pmanager);

    if (pluginname.size())
    {
        if (pmanager->run_plugin(pluginname) ||
            pmanager->run_plugin("myth" + pluginname))
        {
            qApp->exec();

            return GENERIC_EXIT_OK;
        }
        else
            return GENERIC_EXIT_INVALID_CMDLINE;
    }

    MediaMonitor *mon = MediaMonitor::GetMediaMonitor();
    if (mon)
    {
        mon->StartMonitoring();
        mainWindow->installEventFilter(mon);
    }

    NetworkControl *networkControl = NULL;
    if (gCoreContext->GetNumSetting("NetworkControlEnabled", 0))
    {
        int networkPort =
            gCoreContext->GetNumSetting("NetworkControlPort", 6545);
        networkControl = new NetworkControl();

        if (!networkControl->listen(gCoreContext->MythHostAddressAny(),
                                    networkPort))
            LOG(VB_GENERAL, LOG_ERR,
                QString("NetworkControl failed to bind to port %1.")
                    .arg(networkPort));
    }

#ifdef __linux__
#ifdef CONFIG_BINDINGS_PYTHON
    HardwareProfile *profile = new HardwareProfile();
    if (profile && profile->NeedsUpdate())
        profile->SubmitProfile();
    delete profile;
#endif
#endif

    if (!RunMenu(themedir, themename) && !resetTheme(themedir, themename))
    {
        return GENERIC_EXIT_NO_THEME;
    }

#ifndef _MSC_VER
    // Setup handler for USR1 signals to reload theme
    signal(SIGUSR1, &signal_USR1_handler);
    // Setup handler for USR2 signals to restart LIRC
    signal(SIGUSR2, &signal_USR2_handler);
#endif
    ThemeUpdateChecker *themeUpdateChecker = NULL;
    if (gCoreContext->GetNumSetting("ThemeUpdateNofications", 1))
        themeUpdateChecker = new ThemeUpdateChecker();

    MythSystemEventHandler *sysEventHandler = new MythSystemEventHandler();
    GetMythMainWindow()->RegisterSystemEventHandler(sysEventHandler);

    BackendConnectionManager bcm;

    PreviewGeneratorQueue::CreatePreviewGeneratorQueue(
        PreviewGenerator::kRemote, 50, 60);

    int ret = qApp->exec();

    PreviewGeneratorQueue::TeardownPreviewGeneratorQueue();

    if (themeUpdateChecker)
        delete themeUpdateChecker;

    delete sysEventHandler;

    pmanager->DestroyAllPlugins();

    if (mon)
        mon->deleteLater();

    delete networkControl;

    DestroyMythMainWindow();

    return ret;

}

#include "main.moc"
/* vim: set expandtab tabstop=4 shiftwidth=4: */
