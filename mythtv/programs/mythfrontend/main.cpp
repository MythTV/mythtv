
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
#include "mythcommandlineparser.h"
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

static ExitPrompter   *exitPopup = NULL;
static MythThemedMenu *menu;

static QString         logfile;
static MediaRenderer  *g_pUPnp   = NULL;
static MythPluginManager *pmanager = NULL;

static void handleExit(void);

namespace
{
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
            VERBOSE(VB_GENERAL, "Deleting UPnP client...");
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

#ifndef _MSC_VER
        signal(SIGHUP, SIG_DFL);
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

static void TVMenuCallback(void *data, QString &selection)
{
    (void)data;
    QString sel = selection.toLower();

    if (sel.left(9) == "settings ")
    {
        GetMythUI()->AddCurrentLocation("Setup");
        gCoreContext->ActivateSettingsCache(false);
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
    else if (sel == "settings audiogeneral")
    {
        AudioGeneralSettings audiosettings;
        audiosettings.exec();
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
    else if (sel == "tv_status")
        showStatus();
    else if (sel == "exiting_app")
        handleExit();
    else
        VERBOSE(VB_IMPORTANT, "Unknown menu action: " + selection);

    if (sel.left(9) == "settings ")
    {
        GetMythUI()->RemoveCurrentLocation();

        gCoreContext->ActivateSettingsCache(true);
        RemoteSendMessage("CLEAR_SETTINGS_CACHE");

        if (sel == "settings general" ||
            sel == "settings generalrecpriorities")
            ScheduledRecording::signalChange(0);
    }
}

static void handleExit(void)
{
    if (gCoreContext->GetNumSetting("NoPromptOnExit", 1) == 0)
        qApp->quit();
    else
    {
        if (!exitPopup)
            exitPopup = new ExitPrompter();

        exitPopup->handleExit();
    }
}

static bool RunMenu(QString themedir, QString themename)
{
    QByteArray tmp = themedir.toLocal8Bit();
    menu = new MythThemedMenu(
        QString(tmp.constData()), "mainmenu.xml",
        GetMythMainWindow()->GetMainStack(), "mainmenu");

    if (menu->foundTheme())
    {
        VERBOSE(VB_IMPORTANT, QString("Found mainmenu.xml for theme '%1'")
                .arg(themename));
        menu->setCallback(TVMenuCallback, gContext);
        GetMythMainWindow()->GetMainStack()->AddScreen(menu);
        return true;
    }

    VERBOSE(VB_IMPORTANT, QString("Couldn't find mainmenu.xml for theme '%1'")
            .arg(themename));
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
}

static int internal_play_media(const QString &mrl, const QString &plot,
                        const QString &title, const QString &subtitle,
                        const QString &director, int season, int episode,
                        int lenMins, const QString &year)
{
    int res = -1;

    QFile checkFile(mrl);
    if ((!checkFile.exists() && !mrl.startsWith("dvd:")
         && !mrl.startsWith("bd:")
         && !mrl.startsWith("myth:")))
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
        lenMins, (year.toUInt()) ? year.toUInt() : 1900);

    int64_t pos = 0;

    if (pginfo->IsVideoDVD())
    {
        RingBuffer *tmprbuf = RingBuffer::Create(pginfo->GetPathname(), false);

        if (!tmprbuf)
        {
            delete pginfo;
            return res;
        }
        QString name;
        QString serialid;
        if (tmprbuf->IsDVD() &&
            tmprbuf->DVD()->GetNameAndSerialNum(name, serialid))
        {
            QStringList fields = pginfo->QueryDVDBookmark(serialid);
            if (!fields.empty())
            {
                QStringList::Iterator it = fields.begin();
                pos = (int64_t)((*++it).toLongLong() & 0xffffffffLL);
            }
        }
        delete tmprbuf;
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

    VERBOSE(VB_IMPORTANT,
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
        VERBOSE(VB_IMPORTANT, QString("Couldn't find theme '%1'")
                .arg(themename));
        cleanup();
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
     //REG_JUMP("Search Listings", "", "", startSearch);
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

     REG_JUMPEX(QT_TRANSLATE_NOOP("MythControls", "Toggle Show Widget Borders"),
         "", "", setDebugShowBorders, false);
     REG_JUMPEX(QT_TRANSLATE_NOOP("MythControls", "Toggle Show Widget Names"),
         "", "", setDebugShowNames, false);

    TV::InitKeys();

    TV::SetFuncPtr("playbackbox", (void *)PlaybackBox::RunPlaybackBox);
    TV::SetFuncPtr("viewscheduled", (void *)ViewScheduled::RunViewScheduled);
    TV::SetFuncPtr("programguide", (void *)GuideGrid::RunProgramGuide);
    TV::SetFuncPtr("programfinder", (void *)RunProgramFinder);
    TV::SetFuncPtr("scheduleeditor", (void *)ScheduleEditor::RunScheduleEditor);
}


static void signal_USR1_handler(int){
      VERBOSE(VB_GENERAL, "SIG USR1 received, reloading theme");
      RemoteSendMessage("CLEAR_SETTINGS_CACHE");
      gCoreContext->ActivateSettingsCache(false);
      GetMythMainWindow()->JumpTo("Reload Theme");
      gCoreContext->ActivateSettingsCache(true);
}

static void signal_USR2_handler(int)
{
    VERBOSE(VB_GENERAL, "SIG USR2 received, restart LIRC handler");
    GetMythMainWindow()->StartLIRC();
}


static int internal_media_init()
{
    REG_MEDIAPLAYER("Internal", QT_TRANSLATE_NOOP("MythControls",
        "MythTV's native media player."), internal_play_media);
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

static void ShowUsage(const MythCommandLineParser &cmdlineparser)
{
    QString    help  = cmdlineparser.GetHelpString(false);
    QByteArray ahelp = help.toLocal8Bit();

    cerr << "Valid options are: " << endl <<
            "-l or --logfile filename       Writes STDERR and STDOUT messages to filename" << endl <<
            "-r or --reset                  Resets frontend appearance settings and language" << endl <<
            ahelp.constData() <<
            "-p or --prompt                 Always prompt for Mythbackend selection." << endl <<
            "-d or --disable-autodiscovery  Never prompt for Mythbackend selection." << endl <<

            "-u or --upgrade-schema         Allow mythfrontend to upgrade the database schema" << endl <<
            "<plugin>                       Initialize and run this plugin" << endl <<
            endl <<
            "Environment Variables:" << endl <<
            "$MYTHTVDIR                     Set the installation prefix" << endl <<
            "$MYTHCONFDIR                   Set the config dir (instead of ~/.mythtv)" << endl;

}

static int log_rotate(int report_error)
{
    int new_logfd = open(logfile.toLocal8Bit().constData(),
                         O_WRONLY|O_CREAT|O_APPEND, 0664);

    if (new_logfd < 0) {
        /* If we can't open the new logfile, send data to /dev/null */
        if (report_error)
        {
            VERBOSE(VB_IMPORTANT, QString("Cannot open logfile '%1'")
                    .arg(logfile));
            return -1;
        }

        new_logfd = open("/dev/null", O_WRONLY);

        if (new_logfd < 0) {
            /* There's not much we can do, so punt. */
            return -1;
        }
    }

#ifdef WINDOWS_CLOSE_CONSOLE
    // pure Win32 GUI app does not have standard IO streams
    // simply assign the file descriptors to the logfile
    *stdout = *(_fdopen(new_logfd, "w"));
    *stderr = *stdout;
    setvbuf(stdout, NULL, _IOLBF, 256);
#else
    while (dup2(new_logfd, 1) < 0 && errno == EINTR);
    while (dup2(new_logfd, 2) < 0 && errno == EINTR);
    while (close(new_logfd) < 0   && errno == EINTR);
#endif

    return 0;
}

static void log_rotate_handler(int)
{
    log_rotate(0);
}

int main(int argc, char **argv)
{
    bool bPromptForBackend    = false;
    bool bBypassAutoDiscovery = false;
    bool upgradeAllowed = false;

    bool cmdline_err;
    MythCommandLineParser cmdline(
        kCLPOverrideSettingsFile |
        kCLPOverrideSettings     |
        kCLPWindowed             |
        kCLPNoWindowed           |
        kCLPGetSettings          |
        kCLPQueryVersion         |
        kCLPVerbose              |
        kCLPNoUPnP               |
#ifdef USING_X11
        kCLPDisplay              |
#endif // USING_X11
        kCLPExtra                |
        kCLPGeometry);

    // Handle --help before QApplication is created, so that
    // we can print help even if X11 is absent.
    for (int argpos = 1; argpos < argc; ++argpos)
    {
        QString arg(argv[argpos]);
        if (arg == "-h" || arg == "--help" || arg == "--usage")
        {
            ShowUsage(cmdline);
            return GENERIC_EXIT_OK;
        }
    }

    for (int argpos = 1; argpos < argc; ++argpos)
    {
        if (cmdline.PreParse(argc, argv, argpos, cmdline_err))
        {
            if (cmdline_err)
                return GENERIC_EXIT_INVALID_CMDLINE;
            if (cmdline.WantsToExit())
                return GENERIC_EXIT_OK;
        }
    }

#ifdef Q_WS_MACX
    // Without this, we can't set focus to any of the CheckBoxSetting, and most
    // of the MythPushButton widgets, and they don't use the themed background.
    QApplication::setDesktopSettingsAware(false);
#endif
    QApplication a(argc, argv);

    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHFRONTEND);

    QString pluginname;

    QFileInfo finfo(a.argv()[0]);

    QString binname = finfo.baseName();

    VERBOSE(VB_IMPORTANT, QString("%1 version: %2 [%3] www.mythtv.org")
                            .arg(MYTH_APPNAME_MYTHFRONTEND)
                            .arg(MYTH_SOURCE_PATH)
                            .arg(MYTH_SOURCE_VERSION));

    bool ResetSettings = false;

    if (binname.toLower() != "mythfrontend")
        pluginname = binname;

    for (int argpos = 1; argpos < a.argc(); ++argpos)
    {
        if (!strcmp(a.argv()[argpos],"--prompt") ||
            !strcmp(a.argv()[argpos],"-p" ))
        {
            bPromptForBackend = true;
        }
        else if (!strcmp(a.argv()[argpos],"--disable-autodiscovery") ||
                 !strcmp(a.argv()[argpos],"-d" ))
        {
            bBypassAutoDiscovery = true;
        }
        else if (!strcmp(a.argv()[argpos],"-l") ||
            !strcmp(a.argv()[argpos],"--logfile"))
        {
            if (a.argc()-1 > argpos)
            {
                logfile = a.argv()[argpos+1];
                if (logfile.startsWith('-'))
                {
                    cerr << "Invalid or missing argument"
                            " to -l/--logfile option\n";
                    return GENERIC_EXIT_INVALID_CMDLINE;
                }
                else
                {
                    ++argpos;
                }
            }
            else
            {
                cerr << "Missing argument to -l/--logfile option\n";
                return GENERIC_EXIT_INVALID_CMDLINE;
            }
        }
        else if (cmdline.Parse(a.argc(), a.argv(), argpos, cmdline_err))
        {
            if (cmdline_err)
                return GENERIC_EXIT_INVALID_CMDLINE;
            if (cmdline.WantsToExit())
                return GENERIC_EXIT_OK;
        }
    }
    QMap<QString,QString> settingsOverride = cmdline.GetSettingsOverride();

    if (logfile.size())
    {
        if (log_rotate(1) < 0)
            cerr << "cannot open logfile; using stdout/stderr" << endl;
        else
        {
            VERBOSE(VB_IMPORTANT, QString("%1 version: %2 [%3] www.mythtv.org")
                                    .arg(MYTH_APPNAME_MYTHFRONTEND)
                                    .arg(MYTH_SOURCE_PATH)
                                    .arg(MYTH_SOURCE_VERSION));

            signal(SIGHUP, &log_rotate_handler);
        }
    }

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        cerr << "Unable to ignore SIGPIPE\n";

    if (!cmdline.GetDisplay().isEmpty())
    {
        MythUIHelper::SetX11Display(cmdline.GetDisplay());
    }

    if (!cmdline.GetGeometry().isEmpty())
    {
        MythUIHelper::ParseGeometryOverride(cmdline.GetGeometry());
    }

    CleanupGuard callCleanup(cleanup);

    gContext = new MythContext(MYTH_BINARY_VERSION);

    if (cmdline.IsUPnPEnabled())
    {
        g_pUPnp  = new MediaRenderer();
        if (!g_pUPnp->initialized())
        {
            delete g_pUPnp;
            g_pUPnp = NULL;
        }
    }

    // Override settings as early as possible to cover bootstrapped screens
    // such as the language prompt
    settingsOverride = cmdline.GetSettingsOverride();
    if (settingsOverride.size())
    {
        QMap<QString, QString>::iterator it;
        for (it = settingsOverride.begin(); it != settingsOverride.end(); ++it)
        {
            VERBOSE(VB_IMPORTANT, QString("Setting '%1' being forced to '%2'")
                                          .arg(it.key()).arg(*it));
            gCoreContext->OverrideSettingForSession(it.key(), *it);
        }
    }

    if (!gContext->Init(true, bPromptForBackend, bBypassAutoDiscovery))
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    if (!GetMythDB()->HaveSchema())
    {
        if (!InitializeMythSchema())
            return GENERIC_EXIT_DB_ERROR;
    }

    for(int argpos = 1; argpos < a.argc(); ++argpos)
    {
        if (!strcmp(a.argv()[argpos],"-l") ||
            !strcmp(a.argv()[argpos],"--logfile"))
        {
            // Arg processing for logfile already done (before MythContext)
            ++argpos;
        } else if (!strcmp(a.argv()[argpos],"-v") ||
                   !strcmp(a.argv()[argpos],"--verbose"))
        {
            // Arg processing for verbose already done (before MythContext)
            ++argpos;
        }
        else if (!strcmp(a.argv()[argpos],"-r") ||
                 !strcmp(a.argv()[argpos],"--reset"))
        {
            ResetSettings = true;
        }
        else if (!strcmp(a.argv()[argpos],"--prompt") ||
                 !strcmp(a.argv()[argpos],"-p" ))
        {
        }
        else if (!strcmp(a.argv()[argpos],"--disable-autodiscovery") ||
                 !strcmp(a.argv()[argpos],"-d" ))
        {
        }
        else if (!strcmp(a.argv()[argpos],"--upgrade-schema") ||
                 !strcmp(a.argv()[argpos],"-u" ))
        {
            upgradeAllowed = true;
        }
        else if (cmdline.Parse(a.argc(), a.argv(), argpos, cmdline_err))
        {
            if (cmdline_err)
            {
                return GENERIC_EXIT_INVALID_CMDLINE;
            }

            if (cmdline.WantsToExit())
            {
                return GENERIC_EXIT_OK;
            }
        }
        else if ((argpos + 1 == a.argc()) &&
                 (!QString(a.argv()[argpos]).startsWith('-')))
        {
            pluginname = a.argv()[argpos];
        }
        else
        {
            cerr << "Invalid argument: " << a.argv()[argpos] << endl;
            ShowUsage(cmdline);
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
    }

    QStringList settingsQuery = cmdline.GetSettingsQuery();
    if (!settingsQuery.empty())
    {
        QStringList::const_iterator it = settingsQuery.begin();
        for (; it != settingsQuery.end(); ++it)
        {
            QString value = gCoreContext->GetSetting(*it);
            QString out = QString("\tSettings Value : %1 = %2")
                .arg(*it).arg(value);
            cout << out.toLocal8Bit().constData() << endl;
        }
        return GENERIC_EXIT_OK;
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
        gCoreContext->SaveSetting("Language", "");
        gCoreContext->SaveSetting("Country", "");

        return GENERIC_EXIT_OK;
    }

    setuid(getuid());

    VERBOSE(VB_IMPORTANT,
            QString("Enabled verbose msgs: %1").arg(verboseString));

    LCD::SetupLCD();
    if (LCD *lcd = LCD::Get())
        lcd->setupLEDs(RemoteGetRecordingMask);

    MythTranslation::load("mythfrontend");

    QString themename = gCoreContext->GetSetting("Theme", DEFAULT_UI_THEME);

    QString themedir = GetMythUI()->FindThemeDir(themename);
    if (themedir.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, QString("Couldn't find theme '%1'")
                .arg(themename));
        return GENERIC_EXIT_NO_THEME;
    }

    GetMythUI()->LoadQtConfig();

    themename = gCoreContext->GetSetting("Theme", DEFAULT_UI_THEME);
    themedir = GetMythUI()->FindThemeDir(themename);
    if (themedir.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, QString("Couldn't find theme '%1'")
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

    if (!UpgradeTVDatabaseSchema(upgradeAllowed))
    {
        VERBOSE(VB_IMPORTANT,
                "Couldn't upgrade database to new schema, exiting.");
        return GENERIC_EXIT_DB_OUTOFDATE;
    }

    WriteDefaults();

    // Refresh Global/Main Menu keys after DB update in case there was no DB
    // when they were written originally
    mainWindow->ResetKeys();

    InitJumpPoints();

    internal_media_init();

    CleanupMyOldInUsePrograms();

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
        int networkPort = gCoreContext->GetNumSetting("NetworkControlPort", 6545);
        networkControl = new NetworkControl();
        if (!networkControl->listen(QHostAddress::Any,networkPort))
            VERBOSE(VB_IMPORTANT,
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
/* vim: set expandtab tabstop=4 shiftwidth=4: */
