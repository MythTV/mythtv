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
#include "databasebox.h"
#include "playbackbox.h"
#include "playlistcontainer.h"
#include "globalsettings.h"
#include "dbcheck.h"
#include "filescanner.h"
#include "musicplayer.h"
#include "config.h"
#ifndef USING_MINGW
#include "cdrip.h"
#include "importmusic.h"
#endif

// System header (relies on config.h define)
#ifdef HAVE_CDAUDIO
#include <cdaudio.h>
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

static void CheckFreeDBServerFile(void)
{
    QString homeDir = QDir::home().path();

    if (homeDir.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "main.o: You don't have a HOME environment variable. CD lookup will almost certainly not work.");
        return;
    }

    QString filename = homeDir + "/.cdserverrc";
    QFile file(filename);

    if (!file.exists())
    {
#ifdef HAVE_CDAUDIO
        struct cddb_conf cddbconf;
        struct cddb_serverlist list;
        struct cddb_host proxy_host;

        memset(&cddbconf, 0, sizeof(cddbconf));

        cddbconf.conf_access = CDDB_ACCESS_REMOTE;
        list.list_len = 1;
        strncpy(list.list_host[0].host_server.server_name,
                "freedb.freedb.org", 256);
        strncpy(list.list_host[0].host_addressing, "~cddb/cddb.cgi", 256);
        list.list_host[0].host_server.server_port = 80;
        list.list_host[0].host_protocol = CDDB_MODE_HTTP;

        cddb_write_serverlist(cddbconf, list, proxy_host.host_server);
#endif
    }
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

    MythScreenStack *popupStack = GetMythMainWindow()->GetStack("popup stack");
    QString message = QObject::tr("Loading Music. Please wait ...");

    MythUIBusyDialog *busy = new MythUIBusyDialog(message, popupStack,
                                                  "musicscanbusydialog");
    if (busy->Create())
        popupStack->AddScreen(busy, false);
    else
        busy = NULL;

    srand(time(NULL));

    CheckFreeDBServerFile();

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

    //  Load all available info about songs (once!)
    QString startdir = gCoreContext->GetSetting("MusicLocation");
    startdir = QDir::cleanPath(startdir);
    if (!startdir.endsWith("/"))
        startdir += "/";

    Metadata::SetStartdir(startdir);

    Decoder::SetLocationFormatUseTags();

    // Only search music files if a directory was specified & there
    // is no data in the database yet (first run).  Otherwise, user
    // can choose "Setup" option from the menu to force it.
    if (!startdir.isEmpty() && !musicdata_exists)
    {
        FileScanner *fscan = new FileScanner();
        fscan->SearchDir(startdir);
        delete fscan;
    }

    QString paths = gCoreContext->GetSetting("TreeLevels");

    // Set the various track formatting modes
    Metadata::setArtistAndTrackFormats();

    AllMusic *all_music = new AllMusic(paths, startdir);

    //  Load all playlists into RAM (once!)
    PlaylistContainer *all_playlists = new PlaylistContainer(
            all_music, gCoreContext->GetHostName());

    gMusicData->paths = paths;
    gMusicData->startdir = startdir;
    gMusicData->all_playlists = all_playlists;
    gMusicData->all_music = all_music;
    gMusicData->initialized = true;

    while (!gMusicData->all_playlists->doneLoading() || !gMusicData->all_music->doneLoading())
    {
        qApp->processEvents();
        usleep(50000);
    }
    gMusicData->all_playlists->postLoad();

    gPlayer->constructPlaylist();

    if (busy)
        busy->Close();

}

static void startPlayback(void)
{
    loadMusic();
    PlaybackBoxMusic *pbb;
    pbb = new PlaybackBoxMusic(GetMythMainWindow(),
                               "music_play", "music-", chooseCD(), "music_playback");
    pbb->exec();
    qApp->processEvents();

    delete pbb;
}

static void startDatabaseTree(void)
{
    loadMusic();
    DatabaseBox *dbbox = new DatabaseBox(GetMythMainWindow(),
                         chooseCD(), "music_select", "music-", "music database");
    dbbox->exec();
    delete dbbox;

    gPlayer->constructPlaylist();
}

static void startRipper(void)
{
    loadMusic();

#ifndef USING_MINGW
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

static void startImport(void)
{
    loadMusic();

#ifndef USING_MINGW
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    ImportMusicDialog *import = new ImportMusicDialog(mainStack);

    if (import->Create())
        mainStack->AddScreen(import);
    else
        delete import;

    QObject::connect(import, SIGNAL(importFinished()),
                     gMusicData, SLOT(reloadMusic()),
                     Qt::QueuedConnection);
#endif
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
        if ("" != gMusicData->startdir)
        {
            loadMusic();
            FileScanner *fscan = new FileScanner();
            fscan->SearchDir(gMusicData->startdir);
            gMusicData->reloadMusic();
            delete fscan;
        }
    }
    else if (sel == "music_set_general")
    {
        MusicGeneralSettings settings;
        settings.exec();
    }
    else if (sel == "music_set_player")
    {
        MusicPlayerSettings settings;
        settings.exec();
    }
    else if (sel == "music_set_ripper")
    {
        MusicRipperSettings settings;
        settings.exec();
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
        if (class LCD * lcd = LCD::Get())
        {
            lcd->switchToTime();
        }
        GetMythMainWindow()->GetMainStack()->AddScreen(diag);
        return 0;
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("Couldn't find menu %1 or theme %2")
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

#ifndef USING_MINGW
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

static void runScan(void)
{
    loadMusic();

    if ("" != gMusicData->startdir)
    {
        FileScanner *fscan = new FileScanner();
        fscan->SearchDir(gMusicData->startdir);
        gMusicData->reloadMusic();
        delete fscan;
    }
}

static void showMiniPlayer(void)
{
    if (!gMusicData->all_music)
        return;

    // only show the miniplayer if there isn't already a client attached
    if (!gPlayer->hasClient())
        gPlayer->showMiniPlayer();
}

static void handleMedia(MythMediaDevice *cd)
{
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
            VERBOSE(VB_MEDIA, "MythMusic: Forgetting existing CD");
        }
        else
        {
            gCDdevice = newDevice;
            VERBOSE(VB_MEDIA, "MythMusic: Storing CD device " + gCDdevice);
        }
    }
    else
    {
        gCDdevice = QString::null;
        return;
    }

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
    REG_KEY("Music", "FILTER",     QT_TRANSLATE_NOOP("MythControls",
        "Filter All My Music"),        "F");
    REG_KEY("Music", "INCSEARCH",     QT_TRANSLATE_NOOP("MythControls",
        "Show incremental search dialog"),     "Ctrl+S");
    REG_KEY("Music", "INCSEARCHNEXT", QT_TRANSLATE_NOOP("MythControls",
        "Incremental search find next match"), "Ctrl+N");
    REG_KEY("Music", "SPEEDUP",    QT_TRANSLATE_NOOP("MythControls",
        "Increase Play Speed"),   "W");
    REG_KEY("Music", "SPEEDDOWN",  QT_TRANSLATE_NOOP("MythControls",
        "Decrease Play Speed"),   "X");

    REG_MEDIA_HANDLER(QT_TRANSLATE_NOOP("MythControls",
        "MythMusic Media Handler 1/2"), "", "", handleMedia,
        MEDIATYPE_AUDIO | MEDIATYPE_MIXED, QString::null);
    REG_MEDIA_HANDLER(QT_TRANSLATE_NOOP("MythControls",
        "MythMusic Media Handler 2/2"), "", "", handleMedia,
        MEDIATYPE_MMUSIC, "mp3,mp2,ogg,oga,flac,wma,wav,ac3,"
                          "oma,omg,atp,ra,dts,aac,m4a,aa3,tta,"
                          "mka,aiff,swa,wv");
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
        VERBOSE(VB_IMPORTANT,
                "Couldn't upgrade music database schema, exiting.");
        return -1;
    }

    MusicGeneralSettings general;
    general.Load();
    general.Save();

    MusicPlayerSettings settings;
    settings.Load();
    settings.Save();

    MusicRipperSettings ripper;
    ripper.Load();
    ripper.Save();

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
    //TODO do we need this here?
    loadMusic();

    gMusicData->paths = gCoreContext->GetSetting("TreeLevels");
    gMusicData->startdir = gCoreContext->GetSetting("MusicLocation");
    gMusicData->startdir = QDir::cleanPath(gMusicData->startdir);

    if (!gMusicData->startdir.endsWith("/"))
        gMusicData->startdir += "/";

    Metadata::SetStartdir(gMusicData->startdir);

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

    gPlayer->deleteLater();

    delete gMusicData;
}
