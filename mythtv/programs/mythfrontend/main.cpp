
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <cerrno>
#include <pthread.h>

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

#include "mythconfig.h"
#include "tv.h"
#include "proglist.h"
#include "progfind.h"
#include "scheduleeditor.h"
#include "manualschedule.h"
#include "playbackbox.h"
#include "customedit.h"
#include "viewscheduled.h"
#include "programrecpriority.h"
#include "channelrecpriority.h"
#include "custompriority.h"
#include "globalsettings.h"
#include "profilegroup.h"
#include "playgroup.h"
#include "networkcontrol.h"
#include "DVDRingBuffer.h"
#include "scheduledrecording.h"
#include "mythsystemevent.h"

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
#include "mythcommandlineparser.h"
#include "channelgroupsettings.h"

#include "myththemedmenu.h"
#include "myththemebase.h"
#include "mediarenderer.h"
#include "mythmainwindow.h"
#include "mythappearance.h"
#include "mythcontrols.h"
#include "mythuihelper.h"
#include "mythdirs.h"
#include "mythosdmenueditor.h"
#include "audiopulseutil.h"
#include "mythdb.h"
#include "backendconnectionmanager.h"

static ExitPrompter   *exitPopup = NULL;
static MythThemedMenu *menu;
static MythThemeBase  *themeBase = NULL;

static QString         logfile;
static MediaRenderer  *g_pUPnp   = NULL;

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
                    if (buttonnum == 1)
                        pgi->setIgnoreBookmark(true); 
                    else if (buttonnum != 0)
                    {
                        delete pgi;
                        return;
                    }

                    TV::StartTV(pgi);

                    sleep(1);
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

        delete themeBase;
        themeBase = NULL;

        if (g_pUPnp)
        {
            // This takes a few seconds, so inform the user:
            VERBOSE(VB_GENERAL, "Deleting UPnP client...");
            delete g_pUPnp;
            g_pUPnp = NULL;
        }

        delete gContext;
        gContext = NULL;

        signal(SIGHUP, SIG_DFL);
        signal(SIGUSR1, SIG_DFL);
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

void startAppearWiz(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    MythAppearance *mythappearance = new MythAppearance(mainStack,
                                                        "mythappearance");

    if (mythappearance->Create())
        mainStack->AddScreen(mythappearance);
    else
        delete mythappearance;
}


void startKeysSetup()
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    MythControls *mythcontrols = new MythControls(mainStack, "mythcontrols");

    if (mythcontrols->Create())
        mainStack->AddScreen(mythcontrols);
    else
        delete mythcontrols;
}

void startOSDMenuEditor(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    MythOSDMenuEditor *mythosdmenueditor =
        new MythOSDMenuEditor(mainStack, "mythosdmenueditor");

    if (mythosdmenueditor->Create())
        mainStack->AddScreen(mythosdmenueditor);
}


void startGuide(void)
{
    uint chanid = 0;
    QString channum = gContext->GetSetting("DefaultTVChannel");
    channum = (channum.isEmpty()) ? "3" : channum;
    GuideGrid::RunProgramGuide(chanid, channum, NULL, false, true, -2);
}

void startFinder(void)
{
    RunProgramFinder();
}

void startSearchTitle(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plTitleSearch, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

void startSearchKeyword(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plKeywordSearch, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

void startSearchPeople(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plPeopleSearch, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

void startSearchPower(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plPowerSearch, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

void startSearchStored(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plStoredSearch, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

void startSearchChannel(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plChannel, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

void startSearchCategory(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plCategory, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

void startSearchMovie(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plMovies, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

void startSearchNew(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plNewListings, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

void startSearchTime(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack, plTime, "", "");
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

void startManaged(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    ViewScheduled *viewsched = new ViewScheduled(mainStack);

    if (viewsched->Create())
        mainStack->AddScreen(viewsched);
    else
        delete viewsched;
}

void startProgramRecPriorities(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    ProgramRecPriority *progRecPrior = new ProgramRecPriority(mainStack,
                                                        "ProgramRecPriority");

    if (progRecPrior->Create())
        mainStack->AddScreen(progRecPrior);
    else
        delete progRecPrior;
}

void startManageRecordingRules(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    ProgramRecPriority *progRecPrior = new ProgramRecPriority(mainStack,
                                                            "ManageRecRules");

    if (progRecPrior->Create())
        mainStack->AddScreen(progRecPrior);
    else
        delete progRecPrior;
}

void startChannelRecPriorities(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    ChannelRecPriority *chanRecPrior = new ChannelRecPriority(mainStack);

    if (chanRecPrior->Create())
        mainStack->AddScreen(chanRecPrior);
    else
        delete chanRecPrior;
}

void startCustomPriority(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    CustomPriority *custom = new CustomPriority(mainStack);

    if (custom->Create())
        mainStack->AddScreen(custom);
    else
        delete custom;
}

void startPlaybackWithGroup(QString recGroup = "")
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

void startPlayback(void)
{
    startPlaybackWithGroup();
}

void startDelete(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    PlaybackBox *pbb = new PlaybackBox(
        mainStack, "deletebox", PlaybackBox::kDeleteBox);

    if (pbb->Create())
        mainStack->AddScreen(pbb);
    else
        delete pbb;
}

void startPrevious(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    ProgLister *pl = new ProgLister(mainStack);
    if (pl->Create())
        mainStack->AddScreen(pl);
    else
        delete pl;
}

void startCustomEdit(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
    CustomEdit *custom = new CustomEdit(mainStack);

    if (custom->Create())
        mainStack->AddScreen(custom);
    else
        delete custom;
}

void startManualSchedule(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    ManualSchedule *mansched= new ManualSchedule(mainStack);

    if (mansched->Create())
        mainStack->AddScreen(mansched);
    else
        delete mansched;
}

void startTVInGuide(void)
{
    TV::StartTV(NULL, true);
}

void startTVNormal(void)
{
    TV::StartTV(NULL, false);
}

void showStatus(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    StatusBox *statusbox = new StatusBox(mainStack);

    if (statusbox->Create())
        mainStack->AddScreen(statusbox);
    else
        delete statusbox;
}

void TVMenuCallback(void *data, QString &selection)
{
    (void)data;
    QString sel = selection.toLower();

    if (sel.left(9) == "settings ")
    {
        GetMythUI()->AddCurrentLocation("Setup");
        gContext->ActivateSettingsCache(false);
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
    else if (sel == "screensetupwizard")
    {
       startAppearWiz();
    }
    else if (sel == "osdmenueditor")
    {
        startOSDMenuEditor();
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
    else if (sel == "settings maingeneral")
    {
        MainGeneralSettings mainsettings;
        mainsettings.exec();
        menu->ReloadExitKey();
        QStringList strlist( QString("REFRESH_BACKEND") );
        gContext->SendReceiveStringList(strlist);
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

        gContext->ActivateSettingsCache(true);
        RemoteSendMessage("CLEAR_SETTINGS_CACHE");

        if (sel == "settings general" ||
            sel == "settings generalrecpriorities")
            ScheduledRecording::signalChange(0);
    }
}

void handleExit(void)
{
    if (gContext->GetNumSetting("NoPromptOnExit", 1) == 0)
        qApp->quit();
    else
    {
        if (!exitPopup)
            exitPopup = new ExitPrompter();

        exitPopup->handleExit();
    }
}

bool RunMenu(QString themedir, QString themename)
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
void WriteDefaults()
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

int internal_play_media(const QString &mrl, const QString &plot,
                        const QString &title, const QString &subtitle,
                        const QString &director, int season, int episode,
                        int lenMins, const QString &year)
{
    int res = -1;

    QFile checkFile(mrl);
    if ((!checkFile.exists() && !mrl.startsWith("dvd:")
         && !mrl.startsWith("myth:")))
    {
        QString errorText = QObject::tr("Failed to open \n '%1' in %2 \n"
                                        "Check if the video exists")
                                        .arg(mrl.section("/", -1))
                                        .arg(mrl.section("/", 0, -2));
        ShowOkPopup(errorText);
        return res;
    }

    ProgramInfo *pginfo = new ProgramInfo();
    pginfo->recstartts = QDateTime::currentDateTime()
            .addSecs((0 - (lenMins + 1)) * 60 );
    pginfo->recendts = QDateTime::currentDateTime().addSecs(-60);
    pginfo->startts.setDate(QDate::fromString(QString("%1-01-01").arg(year),
                                              Qt::ISODate));
    pginfo->lenMins = lenMins;
    pginfo->isVideo = true;
    pginfo->pathname = mrl;

    QDir d(mrl + "/VIDEO_TS");
    if (mrl.lastIndexOf(".iso", -1, Qt::CaseInsensitive) ==
        (int)mrl.length() - 4 ||
        mrl.lastIndexOf(".img", -1, Qt::CaseInsensitive) ==
        (int)mrl.length() - 4 ||
        d.exists())
    {
        pginfo->pathname = QString("dvd:%1").arg(mrl);
    }

    if (!director.isEmpty())
        pginfo->description = QString( "%1: %2.  " )
                           .arg(QObject::tr("Directed By")).arg(director);

    pginfo->description += plot;

    if (!subtitle.isEmpty())
        pginfo->subtitle = subtitle;

    if ((season > 0) || (episode > 0))
    {
        QString seas, ep;
        seas = QString::number(season);
        ep = QString::number(episode);
        if (ep.size() < 2)
            ep.prepend("0");
        QString SeasEpTitle =  QString("%1x%2").arg(seas).arg(ep);
        pginfo->chanstr = SeasEpTitle;
    }

    pginfo->title = title;

    long long pos = 0;

    if (pginfo->pathname.startsWith("dvd:"))
    {
        bool allowdvdbookmark = gContext->GetNumSetting("EnableDVDBookmark", 0);
        pginfo->setIgnoreBookmark(!allowdvdbookmark);
        if (allowdvdbookmark &&
            gContext->GetNumSetting("DVDBookmarkPrompt", 0))
        {
            RingBuffer *tmprbuf = new RingBuffer(pginfo->pathname, false);

            if (!tmprbuf) 
            { 
                delete pginfo; 
                return res;               
            } 
            QString name;
            QString serialid;
            if (tmprbuf->isDVD() &&
                 tmprbuf->DVD()->GetNameAndSerialNum(name, serialid))
            {
                QStringList fields = pginfo->GetDVDBookmark(serialid, false);
                if (!fields.empty())
                {
                    QStringList::Iterator it = fields.begin();
                    pos = (long long)((*++it).toLongLong() & 0xffffffffLL);
                }
            }
            delete tmprbuf;
        }
    }
    else if (pginfo->isVideo)
        pos = pginfo->GetBookmark(); 
 
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
        TV::StartTV(pginfo);

        res = 0;

        sleep(1);
        delete pginfo;
    }

    return res;
}

void gotoMainMenu(void)
{
    // If we got to this callback, we're back on the menu.  So, send a CTRL-L
    // to cause the menu to reload
    QKeyEvent *event =
        new QKeyEvent(QEvent::KeyPress, Qt::Key_L, Qt::ControlModifier);
    QCoreApplication::postEvent((QObject*)(gContext->GetMainWindow()), event);
}

// If the theme specified in the DB is somehow broken, try a standard one:
//
bool resetTheme(QString themedir, const QString badtheme)
{
    QString themename = DEFAULT_UI_THEME;

    if (badtheme == DEFAULT_UI_THEME)
        themename = FALLBACK_UI_THEME;

    VERBOSE(VB_IMPORTANT,
                QString("Overriding broken theme '%1' with '%2'")
                .arg(badtheme).arg(themename));

    gContext->OverrideSettingForSession("Theme", themename);
    themedir = GetMythUI()->FindThemeDir(themename);

    LanguageSettings::reload();
    GetMythUI()->LoadQtConfig();
    GetMythMainWindow()->Init();
    themeBase->Reload();
    GetMythUI()->UpdateImageCache();

    GetMythMainWindow()->ReinitDone();

    return RunMenu(themedir, themename);
}

int reloadTheme(void)
{
    LanguageSettings::reload();

    GetMythMainWindow()->SetEffectsEnabled(false);

    GetMythUI()->LoadQtConfig();

    GetMythMainWindow()->Init();
    menu->Close();
    GetMythUI()->UpdateImageCache();
    themeBase->Reload();

    GetMythMainWindow()->ReinitDone();

    GetMythMainWindow()->SetEffectsEnabled(true);

    QString themename = gContext->GetSetting("Theme", DEFAULT_UI_THEME);
    QString themedir = GetMythUI()->FindThemeDir(themename);
    if (themedir.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, QString("Couldn't find theme '%1'")
                .arg(themename));
        cleanup();
        return FRONTEND_BUGGY_EXIT_NO_THEME;
    }

    if (!RunMenu(themedir, themename) && !resetTheme(themedir, themename))
        return FRONTEND_BUGGY_EXIT_NO_THEME;

    LCD::SetupLCD();
    if (LCD *lcd = LCD::Get())
    {
        lcd->setupLEDs(RemoteGetRecordingMask);
        lcd->resetServer();
    }

    return 0;
}

void reloadTheme_void(void)
{
    int err = reloadTheme();
    if (err)
        exit(err);
}

void getScreenShot(void)
{
    (void) gContext->GetMainWindow()->screenShot();
}

void InitJumpPoints(void)
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
     REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "TV Recording Playback"),
         "", "", startPlayback);
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

     REG_JUMPEX(QT_TRANSLATE_NOOP("MythControls", "ScreenShot"),
         "", "", getScreenShot, false);

    TV::InitKeys();

    TV::SetFuncPtr("playbackbox", (void *)PlaybackBox::RunPlaybackBox);
    TV::SetFuncPtr("viewscheduled", (void *)ViewScheduled::RunViewScheduled);
    TV::SetFuncPtr("programguide", (void *)GuideGrid::RunProgramGuide);
    TV::SetFuncPtr("programfinder", (void *)RunProgramFinder);
    TV::SetFuncPtr("scheduleeditor", (void *)ScheduleEditor::RunScheduleEditor);
}


void signal_USR1_handler(int){
      VERBOSE(VB_GENERAL, "SIG USR1 received, reloading theme");
      RemoteSendMessage("CLEAR_SETTINGS_CACHE");
      gContext->ActivateSettingsCache(false);
      GetMythMainWindow()->JumpTo("Reload Theme");
      gContext->ActivateSettingsCache(true);
}

void signal_USR2_handler(int)
{
    VERBOSE(VB_GENERAL, "SIG USR2 received, restart LIRC handler");
    GetMythMainWindow()->StartLIRC();
}


int internal_media_init()
{
    REG_MEDIAPLAYER("Internal", QT_TRANSLATE_NOOP("MythControls",
        "MythTV's native media player."), internal_play_media);
    return 0;
}

static void *run_priv_thread(void *data)
{
    VERBOSE(VB_PLAYBACK, QString("user: %1 effective user: %2 run_priv_thread")
                            .arg(getuid()).arg(geteuid()));

    (void)data;
    while (true)
    {
        gContext->waitPrivRequest();

        for (MythPrivRequest req = gContext->popPrivRequest();
             true; req = gContext->popPrivRequest())
        {
            bool done = false;
            switch (req.getType())
            {
            case MythPrivRequest::MythRealtime:
                if (gContext->GetNumSetting("RealtimePriority", 1))
                {
                    pthread_t *target_thread = (pthread_t *)(req.getData());
                    // Raise the given thread to realtime priority
                    struct sched_param sp = {1};
                    if (target_thread)
                    {
                        int status = pthread_setschedparam(
                            *target_thread, SCHED_FIFO, &sp);
                        if (status)
                        {
                            // perror("pthread_setschedparam");
                            VERBOSE(VB_GENERAL, "Realtime priority would"
                                                " require SUID as root.");
                        }
                        else
                            VERBOSE(VB_GENERAL, "Using realtime priority.");
                    }
                    else
                    {
                        VERBOSE(VB_IMPORTANT, "Unexpected NULL thread ptr for"
                                              " MythPrivRequest::MythRealtime");
                    }
                }
                else
                    VERBOSE(VB_GENERAL, "The realtime priority"
                                        " setting is not enabled.");
                break;
            case MythPrivRequest::MythExit:
                pthread_exit(NULL);
                break;
            case MythPrivRequest::PrivEnd:
                done = true; // queue is empty
                break;
            }
            if (done)
                break; // from processing the current queue
        }
    }
    return NULL; // will never happen
}

void CleanupMyOldInUsePrograms(void)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("DELETE FROM inuseprograms "
                  "WHERE hostname = :HOSTNAME and recusage = 'player' ;");
    query.bindValue(":HOSTNAME", gContext->GetHostName());
    if (!query.exec())
        MythDB::DBError("CleanupMyOldInUsePrograms", query);
}

void ShowUsage(const MythCommandLineParser &cmdlineparser)
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

int log_rotate(int report_error)
{
    int new_logfd = open(logfile.toLocal8Bit().constData(),
                         O_WRONLY|O_CREAT|O_APPEND, 0664);

    if (new_logfd < 0) {
        /* If we can't open the new logfile, send data to /dev/null */
        if (report_error)
        {
            VERBOSE(VB_IMPORTANT, QString("Can not open logfile '%1'")
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

void log_rotate_handler(int)
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
            return FRONTEND_EXIT_OK;
        }
    }

    for (int argpos = 1; argpos < argc; ++argpos)
    {
        if (cmdline.PreParse(argc, argv, argpos, cmdline_err))
        {
            if (cmdline_err)
                return FRONTEND_EXIT_INVALID_CMDLINE;
            if (cmdline.WantsToExit())
                return FRONTEND_EXIT_OK;
        }
    }

#ifdef Q_WS_MACX
    // Without this, we can't set focus to any of the CheckBoxSetting, and most
    // of the MythPushButton widgets, and they don't use the themed background.
    QApplication::setDesktopSettingsAware(false);
#endif
    QApplication a(argc, argv);

    QString pluginname;

    QFileInfo finfo(a.argv()[0]);

    QString binname = finfo.baseName();

    extern const char *myth_source_version;
    extern const char *myth_source_path;

    VERBOSE(VB_IMPORTANT, QString("%1 version: %2 [%3] www.mythtv.org")
                            .arg(binname)
                            .arg(myth_source_path)
                            .arg(myth_source_version));

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
                if (logfile.startsWith("-"))
                {
                    cerr << "Invalid or missing argument"
                            " to -l/--logfile option\n";
                    return FRONTEND_EXIT_INVALID_CMDLINE;
                }
                else
                {
                    ++argpos;
                }
            }
            else
            {
                cerr << "Missing argument to -l/--logfile option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }
        }
        else if (cmdline.Parse(a.argc(), a.argv(), argpos, cmdline_err))
        {
            if (cmdline_err)
                return FRONTEND_EXIT_INVALID_CMDLINE;
            if (cmdline.WantsToExit())
                return FRONTEND_EXIT_OK;
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
                                    .arg(binname)
                                    .arg(myth_source_path)
                                    .arg(myth_source_version));

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
    g_pUPnp  = new MediaRenderer();

    if (!gContext->Init(true, g_pUPnp, bPromptForBackend, bBypassAutoDiscovery))
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        return FRONTEND_EXIT_NO_MYTHCONTEXT;
    }

    int pa_ret = pulseaudio_handle_startup();
    if (pa_ret != GENERIC_EXIT_OK)
        return pa_ret;

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
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }

            if (cmdline.WantsToExit())
            {
                return FRONTEND_EXIT_OK;
            }
        }
        else if ((argpos + 1 == a.argc()) &&
                 (!QString(a.argv()[argpos]).startsWith("-")))
        {
            pluginname = a.argv()[argpos];
        }
        else
        {
            cerr << "Invalid argument: " << a.argv()[argpos] << endl;
            ShowUsage(cmdline);
            return FRONTEND_EXIT_INVALID_CMDLINE;
        }
    }
    settingsOverride = cmdline.GetSettingsOverride();

    QStringList settingsQuery = cmdline.GetSettingsQuery();
    if (!settingsQuery.empty())
    {
        QStringList::const_iterator it = settingsQuery.begin();
        for (; it != settingsQuery.end(); ++it)
        {
            QString value = gContext->GetSetting(*it);
            QString out = QString("\tSettings Value : %1 = %2")
                .arg(*it).arg(value);
            cout << out.toLocal8Bit().constData() << endl;
        }
        return FRONTEND_EXIT_OK;
    }

    QString fileprefix = GetConfDir();

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    if (ResetSettings)
    {
        AppearanceSettings as;
        as.Save();

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("update settings set data='EN' "
                      "WHERE hostname = :HOSTNAME and value='Language' ;");
        query.bindValue(":HOSTNAME", gContext->GetHostName());
        if (!query.exec())
            MythDB::DBError("Updating language", query);

        return FRONTEND_EXIT_OK;
    }

    if (settingsOverride.size())
    {
        QMap<QString, QString>::iterator it;
        for (it = settingsOverride.begin(); it != settingsOverride.end(); ++it)
        {
            VERBOSE(VB_IMPORTANT, QString("Setting '%1' being forced to '%2'")
                                          .arg(it.key()).arg(*it));
            gContext->OverrideSettingForSession(it.key(), *it);
        }
    }

    // Create priveleged thread, then drop privs
    pthread_t priv_thread;
    bool priv_thread_created = true;

    VERBOSE(VB_PLAYBACK, QString("user: %1 effective user: %2 before "
                            "privileged thread").arg(getuid()).arg(geteuid()));
    int status = pthread_create(&priv_thread, NULL, run_priv_thread, NULL);
    VERBOSE(VB_PLAYBACK, QString("user: %1 effective user: %2 after "
                            "privileged thread").arg(getuid()).arg(geteuid()));
    if (status)
    {
        VERBOSE(VB_IMPORTANT, QString("Warning: ") +
                "Failed to create priveledged thread." + ENO);
        priv_thread_created = false;
    }
    setuid(getuid());

    VERBOSE(VB_IMPORTANT,
            QString("Enabled verbose msgs: %1").arg(verboseString));

    LCD::SetupLCD();
    if (LCD *lcd = LCD::Get())
        lcd->setupLEDs(RemoteGetRecordingMask);

    LanguageSettings::load("mythfrontend");

    QString themename = gContext->GetSetting("Theme", DEFAULT_UI_THEME);

    QString themedir = GetMythUI()->FindThemeDir(themename);
    if (themedir.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, QString("Couldn't find theme '%1'")
                .arg(themename));
        return FRONTEND_EXIT_NO_THEME;
    }

    GetMythUI()->LoadQtConfig();

    MythMainWindow *mainWindow = GetMythMainWindow();
    mainWindow->Init();
    gContext->SetMainWindow(mainWindow);
    mainWindow->setWindowTitle(QObject::tr("MythTV Frontend"));

    GetMythUI()->UpdateImageCache();
    themeBase = new MythThemeBase();

    LanguageSettings::prompt();

    if (!UpgradeTVDatabaseSchema(upgradeAllowed))
    {
        VERBOSE(VB_IMPORTANT,
                "Couldn't upgrade database to new schema, exiting.");
        return FRONTEND_EXIT_DB_OUTOFDATE;
    }

    WriteDefaults();

    InitJumpPoints();

    internal_media_init();

    CleanupMyOldInUsePrograms();

    MythPluginManager *pmanager = new MythPluginManager();
    gContext->SetPluginManager(pmanager);

    if (pluginname.size())
    {
        if (pmanager->run_plugin(pluginname) ||
            pmanager->run_plugin("myth" + pluginname))
        {
            qApp->exec();

            return FRONTEND_EXIT_OK;
        }
        else
            return FRONTEND_EXIT_INVALID_CMDLINE;
    }

    MediaMonitor *mon = MediaMonitor::GetMediaMonitor();
    if (mon)
    {
        mon->StartMonitoring();
        mainWindow->installEventFilter(mon);
    }

    NetworkControl *networkControl = NULL;
    if (gContext->GetNumSetting("NetworkControlEnabled", 0))
    {
        int networkPort = gContext->GetNumSetting("NetworkControlPort", 6545);
        networkControl = new NetworkControl();
        if (!networkControl->listen(QHostAddress::Any,networkPort))
            VERBOSE(VB_IMPORTANT,
                    QString("NetworkControl failed to bind to port %1.")
                    .arg(networkPort));
    }

    themename = gContext->GetSetting("Theme", DEFAULT_UI_THEME);
    themedir = GetMythUI()->FindThemeDir(themename);
    if (themedir.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, QString("Couldn't find theme '%1'")
                .arg(themename));
        return FRONTEND_EXIT_NO_THEME;
    }

    if (!RunMenu(themedir, themename) && !resetTheme(themedir, themename))
    {
        delete networkControl;
        return FRONTEND_EXIT_NO_THEME;
    }

    // Setup handler for USR1 signals to reload theme
    signal(SIGUSR1, &signal_USR1_handler);
    // Setup handler for USR2 signals to restart LIRC
    signal(SIGUSR2, &signal_USR2_handler);

    MythSystemEventHandler *sysEventHandler = new MythSystemEventHandler();
    GetMythMainWindow()->RegisterSystemEventHandler(sysEventHandler);

    BackendConnectionManager bcm;

    int ret = qApp->exec();

    if (sysEventHandler)
        delete sysEventHandler;

    pmanager->DestroyAllPlugins();

    if (mon)
        mon->deleteLater();

    if (priv_thread_created)
    {
        gContext->addPrivRequest(MythPrivRequest::MythExit, NULL);
        pthread_join(priv_thread, NULL);
    }

    delete networkControl;

    DestroyMythMainWindow();

    pa_ret = pulseaudio_handle_teardown();
    if (GENERIC_EXIT_OK != pa_ret)
        return pa_ret;

    return ret;

}
/* vim: set expandtab tabstop=4 shiftwidth=4: */
