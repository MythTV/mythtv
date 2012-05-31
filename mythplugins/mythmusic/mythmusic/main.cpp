// POSIX headers
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>

// Qt headers
#include <QDir>
#include <QApplication>
#include <QRegExp>

// MythTV headers
#include <mythcontext.h>
#include <mythplugin.h>
#include <mythmediamonitor.h>
#include <mythdbcon.h>
#include <mythdb.h>
#include <mythpluginapi.h>
#include <mythversion.h>
#include <myththemedmenu.h>
#include <compat.h>
#include <mythuihelper.h>
#include <mythprogressdialog.h>
#include <lcddevice.h>

// MythMusic headers
#include "decoder.h"
#include "metadata.h"
#include "playlisteditorview.h"
#include "playlistview.h"
#include "playlistcontainer.h"
#include "dbcheck.h"
#include "filescanner.h"
#include "musicplayer.h"
#include "config.h"
#include "mainvisual.h"
#include "generalsettings.h"
#include "playersettings.h"
#include "visualizationsettings.h"
#include "importsettings.h"
#include "ratingsettings.h"
#include "importmusic.h"

#ifdef HAVE_CDIO
#include "cdrip.h"
#endif


// This stores the last MythMediaDevice that was detected:
QString gCDdevice;

/**
 * \brief Work out the best CD drive to use at this time
 */
static QString chooseCD(void)
{
    if (gCDdevice.length())
        return gCDdevice;

#ifdef Q_OS_MAC
    return MediaMonitor::GetMountPath(MediaMonitor::defaultCDdevice());
#endif

    return MediaMonitor::defaultCDdevice();
}

static void SavePending(int pending)
{
    //  Temporary Hack until mythmusic
    //  has a proper settings/setup

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT * FROM settings "
                  "WHERE value = :LASTPUSH "
                  "AND hostname = :HOST ;");
    query.bindValue(":LASTPUSH", "LastMusicPlaylistPush");
    query.bindValue(":HOST", gCoreContext->GetHostName());

    if (query.exec() && query.size() == 0)
    {
        //  first run from this host / recent version
        query.prepare("INSERT INTO settings (value,data,hostname) VALUES "
                         "(:LASTPUSH, :DATA, :HOST );");
        query.bindValue(":LASTPUSH", "LastMusicPlaylistPush");
        query.bindValue(":DATA", pending);
        query.bindValue(":HOST", gCoreContext->GetHostName());

        if (!query.exec())
            MythDB::DBError("SavePending - inserting LastMusicPlaylistPush",
                            query);
    }
    else if (query.size() == 1)
    {
        //  ah, just right
        query.prepare("UPDATE settings SET data = :DATA "
                         "WHERE value = :LASTPUSH "
                         "AND hostname = :HOST ;");
        query.bindValue(":DATA", pending);
        query.bindValue(":LASTPUSH", "LastMusicPlaylistPush");
        query.bindValue(":HOST", gCoreContext->GetHostName());

        if (!query.exec())
            MythDB::DBError("SavePending - updating LastMusicPlaylistPush",
                            query);
    }
    else
    {
        //  correct thor's diabolical plot to
        //  consume all table space

        query.prepare("DELETE FROM settings WHERE "
                         "WHERE value = :LASTPUSH "
                         "AND hostname = :HOST ;");
        query.bindValue(":LASTPUSH", "LastMusicPlaylistPush");
        query.bindValue(":HOST", gCoreContext->GetHostName());
        if (!query.exec())
            MythDB::DBError("SavePending - deleting LastMusicPlaylistPush",
                            query);

        query.prepare("INSERT INTO settings (value,data,hostname) VALUES "
                         "(:LASTPUSH, :DATA, :HOST );");
        query.bindValue(":LASTPUSH", "LastMusicPlaylistPush");
        query.bindValue(":DATA", pending);
        query.bindValue(":HOST", gCoreContext->GetHostName());

        if (!query.exec())
            MythDB::DBError("SavePending - inserting LastMusicPlaylistPush (2)",
                            query);
    }
}

static void loadMusic()
{
    // only do this once
    if (gMusicData->initialized)
        return;

    MSqlQuery count_query(MSqlQuery::InitCon());

    bool musicdata_exists = false;
    if (count_query.exec("SELECT COUNT(*) FROM music_songs;"))
    {
        if(count_query.next() &&
            0 != count_query.value(0).toInt())
        {
            musicdata_exists = true;
        }
    }

    QString startdir = gCoreContext->GetSetting("MusicLocation");
    startdir = QDir::cleanPath(startdir);
    if (!startdir.isEmpty() && !startdir.endsWith("/"))
        startdir += "/";

    gMusicData->musicDir = startdir;

    Decoder::SetLocationFormatUseTags();

    // Only search music files if a directory was specified & there
    // is no data in the database yet (first run).  Otherwise, user
    // can choose "Setup" option from the menu to force it.
    if (!gMusicData->musicDir.isEmpty() && !musicdata_exists)
    {
        FileScanner *fscan = new FileScanner();
        fscan->SearchDir(startdir);
        delete fscan;
    }

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    QString message = QObject::tr("Loading Music. Please wait ...");

    MythUIBusyDialog *busy = new MythUIBusyDialog(message, popupStack,
                                                  "musicscanbusydialog");
    if (busy->Create())
        popupStack->AddScreen(busy, false);
    else
        busy = NULL;

    // Set the various track formatting modes
    Metadata::setArtistAndTrackFormats();

    AllMusic *all_music = new AllMusic();

    //  Load all playlists into RAM (once!)
    PlaylistContainer *all_playlists = new PlaylistContainer(
            all_music, gCoreContext->GetHostName());

    gMusicData->all_playlists = all_playlists;
    gMusicData->all_music = all_music;
    gMusicData->initialized = true;

    while (!gMusicData->all_playlists->doneLoading() || !gMusicData->all_music->doneLoading())
    {
        qApp->processEvents();
        usleep(50000);
    }
    gMusicData->all_playlists->postLoad();

    gPlayer->loadPlaylist();

    if (busy)
        busy->Close();

}

static void startPlayback(void)
{
    loadMusic();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    PlaylistView *view = new PlaylistView(mainStack);

    if (view->Create())
        mainStack->AddScreen(view);
    else
        delete view;
}

static void startDatabaseTree(void)
{
    loadMusic();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    QString lastView = gCoreContext->GetSetting("MusicPlaylistEditorView", "tree");
    PlaylistEditorView *view = new PlaylistEditorView(mainStack, lastView);

    if (view->Create())
        mainStack->AddScreen(view);
    else
        delete view;
}

static void startRipper(void)
{
    loadMusic();

#if defined HAVE_CDIO
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    Ripper *rip = new Ripper(mainStack, chooseCD());

    if (rip->Create())
    {
        mainStack->AddScreen(rip);
        QObject::connect(rip, SIGNAL(ripFinished()),
                     gMusicData, SLOT(reloadMusic()),
                     Qt::QueuedConnection);
    }
    else
        delete rip;
#endif
}

static void runScan(void)
{
    // maybe we haven't loaded the music yet in which case we wont have a valid music dir set
    if (gMusicData->musicDir.isEmpty())
    {
        QString startdir = gCoreContext->GetSetting("MusicLocation");
        startdir = QDir::cleanPath(startdir);
        if (!startdir.isEmpty() && !startdir.endsWith("/"))
            startdir += "/";

        gMusicData->musicDir = startdir;
    }

    // if we still don't have a valid start dir warn the user and give up
    if (gMusicData->musicDir.isEmpty())
    {
        ShowOkPopup(QObject::tr("You need to tell me where to find your music on the "
                                "'General Settings' page of MythMusic's settings pages."));
       return;
    }

    if (!QFile::exists(gMusicData->musicDir))
    {
        ShowOkPopup(QObject::tr("Can't find your music directory. Have you set it correctly on the "
                                "'General Settings' page of MythMusic's settings pages?"));
       return;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Scanning '%1' for music files").arg(gMusicData->musicDir));

    FileScanner *fscan = new FileScanner();
    fscan->SearchDir(gMusicData->musicDir);

    // save anything that may have changed
    if (gMusicData->all_music && gMusicData->all_music->cleanOutThreads())
        gMusicData->all_music->save();

    if (gMusicData->all_playlists && gMusicData->all_playlists->cleanOutThreads())
    {
        gMusicData->all_playlists->save();
        int x = gMusicData->all_playlists->getPending();
        SavePending(x);
    }

    // force a complete reload of the tracks and playlists
    gPlayer->stop(true);
    delete gMusicData;

    gMusicData = new MusicData;
    loadMusic();

    delete fscan;
}

static void startImport(void)
{
    loadMusic();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    ImportMusicDialog *import = new ImportMusicDialog(mainStack);

    if (import->Create())
    {
        mainStack->AddScreen(import);
        QObject::connect(import, SIGNAL(importFinished()),
                gMusicData, SLOT(reloadMusic()),
                Qt::QueuedConnection);
    }
    else
        delete import;
}

static void MusicCallback(void *data, QString &selection)
{
    (void) data;

    QString sel = selection.toLower();
    if (sel == "music_create_playlist")
        startDatabaseTree();
    else if (sel == "music_play")
        startPlayback();
    else if (sel == "music_rip")
    {
        startRipper();
    }
    else if (sel == "music_import")
    {
        startImport();
    }
    else if (sel == "settings_scan")
    {
        runScan();
    }
    else if (sel == "settings_general")
     {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        GeneralSettings *gs = new GeneralSettings(mainStack, "general settings");

        if (gs->Create())
            mainStack->AddScreen(gs);
        else
            delete gs;
    }
    else if (sel == "settings_player")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        PlayerSettings *ps = new PlayerSettings(mainStack, "player settings");

        if (ps->Create())
            mainStack->AddScreen(ps);
        else
            delete ps;
    }
    else if (sel == "settings_rating")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        RatingSettings *rs = new RatingSettings(mainStack, "rating settings");

        if (rs->Create())
            mainStack->AddScreen(rs);
        else
            delete rs;
    }
    else if (sel == "settings_visualization")
    {

       MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
       VisualizationSettings *vs = new VisualizationSettings(mainStack, "visualization settings");

       if (vs->Create())
           mainStack->AddScreen(vs);
        else
            delete vs;
    }
    else if (sel == "settings_import")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        ImportSettings *is = new ImportSettings(mainStack, "import settings");

        if (is->Create())
            mainStack->AddScreen(is);
        else
            delete is;
    }
}

static int runMenu(QString which_menu)
{
    QString themedir = GetMythUI()->GetThemeDir();

    MythThemedMenu *diag = new MythThemedMenu(
        themedir, which_menu, GetMythMainWindow()->GetMainStack(),
        "music menu");

    diag->setCallback(MusicCallback, NULL);
    diag->setKillable();

    if (diag->foundTheme())
    {
        if (LCD *lcd = LCD::Get())
        {
            lcd->switchToTime();
        }
        GetMythMainWindow()->GetMainStack()->AddScreen(diag);
        return 0;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Couldn't find menu %1 or theme %2")
                              .arg(which_menu).arg(themedir));
        delete diag;
        return -1;
    }
}

static void runMusicPlayback(void)
{
    GetMythUI()->AddCurrentLocation("playmusic");
    startPlayback();
    GetMythUI()->RemoveCurrentLocation();
}

static void runMusicSelection(void)
{
    GetMythUI()->AddCurrentLocation("musicplaylists");
    startDatabaseTree();
    GetMythUI()->RemoveCurrentLocation();
}

static void runRipCD(void)
{
    loadMusic();

#if defined HAVE_CDIO
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    Ripper *rip = new Ripper(mainStack, chooseCD());

    if (rip->Create())
        mainStack->AddScreen(rip);
    else
        delete rip;

    QObject::connect(rip, SIGNAL(ripFinished()),
                     gMusicData, SLOT(reloadMusic()),
                     Qt::QueuedConnection);
#endif
}

static void showMiniPlayer(void)
{
    if (!gMusicData->all_music)
        return;

    // only show the miniplayer if there isn't already a client attached
    if (!gPlayer->hasClient())
        gPlayer->showMiniPlayer();
}

#ifdef FIXME  // the only call is likewise commented out and needs fixing
static void handleMedia(MythMediaDevice *cd)
{
    // if the music player is already playing ignore the event
    if (gPlayer->isPlaying())
    {
        LOG(VB_GENERAL, LOG_NOTICE, "Got a media changed event but ignoring since "
                                      "the music player is already playing");
        return;
    }
    else
         LOG(VB_GENERAL, LOG_NOTICE, "Got a media changed event");

    // Note that we should deal with other disks that may contain music.
    // e.g. MEDIATYPE_MMUSIC or MEDIATYPE_MIXED

    if (!cd)
        return;

    if (cd->isUsable())
    {
        QString newDevice;

#ifdef Q_OS_MAC
        newDevice = cd->getMountPath();
#else
        newDevice = cd->getDevicePath();
#endif

        if (gCDdevice.length() && gCDdevice != newDevice)
        {
            // In the case of multiple audio CDs, clear the old stored device
            // so the user has to choose (via MediaMonitor::defaultCDdevice())

            gCDdevice = QString::null;
            LOG(VB_MEDIA, LOG_INFO, "MythMusic: Forgetting existing CD");
        }
        else
        {
            gCDdevice = newDevice;
            LOG(VB_MEDIA, LOG_INFO,
                "MythMusic: Storing CD device " + gCDdevice);
        }
    }
    else
    {
        gCDdevice = QString::null;
        return;
    }

    GetMythMainWindow()->JumpTo("Main Menu");

    if (gCoreContext->GetNumSetting("AutoPlayCD", 0))
    {
        // Empty the playlist to ensure CD is played first
        if (gMusicData->all_music)
            gMusicData->all_music->clearCDData();
        if (gMusicData->all_playlists)
            gMusicData->all_playlists->clearCDList();

        runMusicPlayback();
    }
    else
        mythplugin_run();
}
#endif

static void setupKeys(void)
{
    REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Play music"),
        "", "", runMusicPlayback);
    REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Select music playlists"),
        "", "", runMusicSelection);
    REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Rip CD"),
        "", "", runRipCD);
    REG_JUMP(QT_TRANSLATE_NOOP("MythControls", "Scan music"),
        "", "", runScan);
    REG_JUMPEX(QT_TRANSLATE_NOOP("MythControls", "Show Music Miniplayer"),
        "", "", showMiniPlayer, false);

    REG_KEY("Music", "NEXTTRACK",  QT_TRANSLATE_NOOP("MythControls",
        "Move to the next track"),     ">,.,Z,End");
    REG_KEY("Music", "PREVTRACK",  QT_TRANSLATE_NOOP("MythControls",
        "Move to the previous track"), ",,<,Q,Home");
    REG_KEY("Music", "FFWD",       QT_TRANSLATE_NOOP("MythControls",
        "Fast forward"),               "PgDown");
    REG_KEY("Music", "RWND",       QT_TRANSLATE_NOOP("MythControls",
        "Rewind"),                     "PgUp");
    REG_KEY("Music", "PAUSE",      QT_TRANSLATE_NOOP("MythControls",
        "Pause/Start playback"),       "P");
    REG_KEY("Music", "PLAY",       QT_TRANSLATE_NOOP("MythControls",
        "Start playback"),             "");
    REG_KEY("Music", "STOP",       QT_TRANSLATE_NOOP("MythControls",
        "Stop playback"),              "O");
    REG_KEY("Music", "VOLUMEDOWN", QT_TRANSLATE_NOOP("MythControls",
        "Volume down"),       "[,{,F10,Volume Down");
    REG_KEY("Music", "VOLUMEUP",   QT_TRANSLATE_NOOP("MythControls",
        "Volume up"),         "],},F11,Volume Up");
    REG_KEY("Music", "MUTE",       QT_TRANSLATE_NOOP("MythControls",
        "Mute"),              "|,\\,F9,Volume Mute");
    REG_KEY("Music", "TOGGLEUPMIX",QT_TRANSLATE_NOOP("MythControls",
        "Toggle audio upmixer"),    "Ctrl+U");
    REG_KEY("Music", "CYCLEVIS",   QT_TRANSLATE_NOOP("MythControls",
        "Cycle visualizer mode"),      "6");
    REG_KEY("Music", "BLANKSCR",   QT_TRANSLATE_NOOP("MythControls",
        "Blank screen"),               "5");
    REG_KEY("Music", "THMBUP",     QT_TRANSLATE_NOOP("MythControls",
        "Increase rating"),            "9");
    REG_KEY("Music", "THMBDOWN",   QT_TRANSLATE_NOOP("MythControls",
        "Decrease rating"),            "7");
    REG_KEY("Music", "REFRESH",    QT_TRANSLATE_NOOP("MythControls",
        "Refresh music tree"),         "8");
    REG_KEY("Music", "SPEEDUP",    QT_TRANSLATE_NOOP("MythControls",
        "Increase Play Speed"),   "W");
    REG_KEY("Music", "SPEEDDOWN",  QT_TRANSLATE_NOOP("MythControls",
        "Decrease Play Speed"),   "X");
    REG_KEY("Music", "MARK",       QT_TRANSLATE_NOOP("MythControls",
        "Toggle track selection"), "T");
    REG_KEY("Music", "TOGGLESHUFFLE", QT_TRANSLATE_NOOP("MythControls",
        "Toggle shuffle mode"),    "");
    REG_KEY("Music", "TOGGLEREPEAT",  QT_TRANSLATE_NOOP("MythControls",
        "Toggle repeat mode"),     "");

    // switch to view key bindings
    REG_KEY("Music", "SWITCHTOPLAYLIST",              QT_TRANSLATE_NOOP("MythControls",
        "Switch to the current playlist view"), "");
    REG_KEY("Music", "SWITCHTOPLAYLISTEDITORTREE",    QT_TRANSLATE_NOOP("MythControls",
        "Switch to the playlist editor tree view"), "");
    REG_KEY("Music", "SWITCHTOPLAYLISTEDITORGALLERY", QT_TRANSLATE_NOOP("MythControls",
        "Switch to the playlist editor gallery view"), "");
    REG_KEY("Music", "SWITCHTOSEARCH",                QT_TRANSLATE_NOOP("MythControls",
        "Switch to the search view"), "");
    REG_KEY("Music", "SWITCHTOVISUALISER",            QT_TRANSLATE_NOOP("MythControls",
        "Switch to the fullscreen visualiser view"), "");


#ifdef FIXME
// FIXME need to find a way to stop the media monitor jumping to the main menu before
// calling the handler
    REG_MEDIA_HANDLER(QT_TRANSLATE_NOOP("MythControls",
        "MythMusic Media Handler 1/2"), "", "", handleMedia,
        MEDIATYPE_AUDIO | MEDIATYPE_MIXED, QString::null);
    REG_MEDIA_HANDLER(QT_TRANSLATE_NOOP("MythControls",
        "MythMusic Media Handler 2/2"), "", "", handleMedia,
        MEDIATYPE_MMUSIC, "mp3,mp2,ogg,oga,flac,wma,wav,ac3,"
                          "oma,omg,atp,ra,dts,aac,m4a,aa3,tta,"
                          "mka,aiff,swa,wv");
#endif
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythmusic", libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    gCoreContext->ActivateSettingsCache(false);
    bool upgraded = UpgradeMusicDatabaseSchema();
    gCoreContext->ActivateSettingsCache(true);

    if (!upgraded)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "Couldn't upgrade music database schema, exiting.");
        return -1;
    }

    setupKeys();

    Decoder::SetLocationFormatUseTags();

    gPlayer = new MusicPlayer(NULL, chooseCD());
    gMusicData = new MusicData();

    return 0;
}


int mythplugin_run(void)
{
    return runMenu("musicmenu.xml");
}

int mythplugin_config(void)
{
    Decoder::SetLocationFormatUseTags();

    return runMenu("music_settings.xml");
}

void mythplugin_destroy(void)
{
    gPlayer->stop(true);

    // TODO these should be saved when they are changed
    // Automagically save all playlists and metadata (ratings) that have changed
    if (gMusicData->all_music && gMusicData->all_music->cleanOutThreads())
    {
        gMusicData->all_music->save();
    }

    if (gMusicData->all_playlists && gMusicData->all_playlists->cleanOutThreads())
    {
        gMusicData->all_playlists->save();
        int x = gMusicData->all_playlists->getPending();
        SavePending(x);
    }

    delete gPlayer;

    delete gMusicData;
}
