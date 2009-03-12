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
#include <mythtv/mythcontext.h>
#include <mythtv/mythplugin.h>
#include <mythtv/mythmediamonitor.h>
#include <mythtv/mythdbcon.h>
#include <mythtv/mythpluginapi.h>
#include <mythtv/mythversion.h>
#include <mythtv/libmythui/myththemedmenu.h>
#include <mythtv/compat.h>
#include <mythtv/libmythui/mythuihelper.h>

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
QString chooseCD(void)
{
    if (gCDdevice.length())
        return gCDdevice;

    return MediaMonitor::defaultCDdevice();
}

void CheckFreeDBServerFile(void)
{
    QString homeDir = QDir::home().path();

    if (homeDir == "")
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

void SavePending(int pending)
{
    //  Temporary Hack until mythmusic
    //  has a proper settings/setup

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT * FROM settings "
                  "WHERE value = :LASTPUSH "
                  "AND hostname = :HOST ;");
    query.bindValue(":LASTPUSH", "LastMusicPlaylistPush");
    query.bindValue(":HOST", gContext->GetHostName());

    if (query.exec() && query.size() == 0)
    {
        //  first run from this host / recent version
        query.prepare("INSERT INTO settings (value,data,hostname) VALUES "
                         "(:LASTPUSH, :DATA, :HOST );");
        query.bindValue(":LASTPUSH", "LastMusicPlaylistPush");
        query.bindValue(":DATA", pending);
        query.bindValue(":HOST", gContext->GetHostName());

        query.exec();
    }
    else if (query.size() == 1)
    {
        //  ah, just right
        query.prepare("UPDATE settings SET data = :DATA WHERE "
                         "WHERE value = :LASTPUSH "
                         "AND hostname = :HOST ;");
        query.bindValue(":DATA", pending);
        query.bindValue(":LASTPUSH", "LastMusicPlaylistPush");
        query.bindValue(":HOST", gContext->GetHostName());

        query.exec();
    }
    else
    {
        //  correct thor's diabolical plot to
        //  consume all table space

        query.prepare("DELETE FROM settings WHERE "
                         "WHERE value = :LASTPUSH "
                         "AND hostname = :HOST ;");
        query.bindValue(":LASTPUSH", "LastMusicPlaylistPush");
        query.bindValue(":HOST", gContext->GetHostName());
        query.exec();

        query.prepare("INSERT INTO settings (value,data,hostname) VALUES "
                         "(:LASTPUSH, :DATA, :HOST );");
        query.bindValue(":LASTPUSH", "LastMusicPlaylistPush");
        query.bindValue(":DATA", pending);
        query.bindValue(":HOST", gContext->GetHostName());

        query.exec();
    }
}

void startPlayback(void)
{
    PlaybackBoxMusic *pbb;
    pbb = new PlaybackBoxMusic(gContext->GetMainWindow(),
                               "music_play", "music-", chooseCD(), "music_playback");
    pbb->exec();
    qApp->processEvents();

    delete pbb;
}

void startDatabaseTree(void)
{
    DatabaseBox *dbbox = new DatabaseBox(gContext->GetMainWindow(),
                         chooseCD(), "music_select", "music-", "music database");
    dbbox->exec();
    delete dbbox;

    gPlayer->constructPlaylist();
}

void startRipper(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    Ripper *rip = new Ripper(mainStack, chooseCD());

    if (rip->Create())
        mainStack->AddScreen(rip);
    else
        delete rip;

//   connect(rip, SIGNAL(Success()), SLOT(RebuildMusicTree()));
}

void startImport(void)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    ImportMusicDialog *import = new ImportMusicDialog(mainStack);

    if (import->Create())
        mainStack->AddScreen(import);
    else
        delete import;

//   connect(import, SIGNAL(Changed()), SLOT(RebuildMusicTree()));
}

void RebuildMusicTree(void)
{
    if (!gMusicData->all_music || !gMusicData->all_playlists)
        return;

    MythBusyDialog *busy = new MythBusyDialog(
        QObject::tr("Rebuilding music tree"));

    busy->start();
    gMusicData->all_music->startLoading();
    while (!gMusicData->all_music->doneLoading())
    {
        qApp->processEvents();
        usleep(50000);
    }
    gMusicData->all_playlists->postLoad();
    busy->Close();
    busy->deleteLater();
}

static void postMusic(void);

void MusicCallback(void *data, QString &selection)
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
            FileScanner *fscan = new FileScanner();
            fscan->SearchDir(gMusicData->startdir);
            RebuildMusicTree();
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
    else if (sel == "exiting_menu")
    {
        if (gMusicData)
        {
            if (gMusicData->runPost)
                postMusic();
        }
    }
}

void runMenu(QString which_menu)
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
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("Couldn't find menu %1 or theme %2")
                              .arg(which_menu).arg(themedir));
        delete diag;
    }
}

void runMusicPlayback(void);
void runMusicSelection(void);
void runRipCD(void);
void runScan(void);
void showMiniPlayer(void);

void handleMedia(MythMediaDevice *cd)
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

    if (gContext->GetNumSetting("AutoPlayCD", 0))
        runMusicPlayback();
    else
        mythplugin_run();
}

void setupKeys(void)
{
    REG_JUMP("Play music",             "", "", runMusicPlayback);
    REG_JUMP("Select music playlists", "", "", runMusicSelection);
    REG_JUMP("Rip CD",                 "", "", runRipCD);
    REG_JUMP("Scan music",             "", "", runScan);
    REG_JUMPEX("Show Music Miniplayer","", "", showMiniPlayer, false);

    REG_KEY("Music", "DELETE",     "Delete track from playlist", "D");
    REG_KEY("Music", "NEXTTRACK",  "Move to the next track",     ">,.,Z,End");
    REG_KEY("Music", "PREVTRACK",  "Move to the previous track", ",,<,Q,Home");
    REG_KEY("Music", "FFWD",       "Fast forward",               "PgDown");
    REG_KEY("Music", "RWND",       "Rewind",                     "PgUp");
    REG_KEY("Music", "PAUSE",      "Pause/Start playback",       "P");
    REG_KEY("Music", "PLAY",       "Start playback",             "");
    REG_KEY("Music", "STOP",       "Stop playback",              "O");
    REG_KEY("Music", "VOLUMEDOWN", "Volume down",       "[,{,F10,Volume Down");
    REG_KEY("Music", "VOLUMEUP",   "Volume up",         "],},F11,Volume Up");
    REG_KEY("Music", "MUTE",       "Mute",              "|,\\,F9,Volume Mute");
    REG_KEY("Music", "CYCLEVIS",   "Cycle visualizer mode",      "6");
    REG_KEY("Music", "BLANKSCR",   "Blank screen",               "5");
    REG_KEY("Music", "THMBUP",     "Increase rating",            "9");
    REG_KEY("Music", "THMBDOWN",   "Decrease rating",            "7");
    REG_KEY("Music", "REFRESH",    "Refresh music tree",         "8");
    REG_KEY("Music", "FILTER",     "Filter All My Music",        "F");
    REG_KEY("Music", "INCSEARCH",     "Show incremental search dialog",     "Ctrl+S");
    REG_KEY("Music", "INCSEARCHNEXT", "Incremental search find next match", "Ctrl+N");
    REG_KEY("Music", "SPEEDUP",    "Increase Play Speed",   "W");
    REG_KEY("Music", "SPEEDDOWN",  "Decrease Play Speed",   "X");

    REG_MEDIA_HANDLER("MythMusic Media Handler 1/2", "", "", handleMedia,
                      MEDIATYPE_AUDIO | MEDIATYPE_MIXED, QString::null);
    REG_MEDIA_HANDLER("MythMusic Media Handler 2/2", "", "", handleMedia,
                      MEDIATYPE_MMUSIC, "ogg,mp3,aac,flac");
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythmusic", libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    gContext->ActivateSettingsCache(false);
    if (!UpgradeMusicDatabaseSchema())
    {
        VERBOSE(VB_IMPORTANT,
                "Couldn't upgrade database to new schema, exiting.");
        return -1;
    }
    gContext->ActivateSettingsCache(true);

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

static void preMusic()
{
    srand(time(NULL));

    CheckFreeDBServerFile();

    MSqlQuery count_query(MSqlQuery::InitCon());
    count_query.exec("SELECT COUNT(*) FROM music_songs;");

    bool musicdata_exists = false;
    if (count_query.isActive())
    {
        if(count_query.next() &&
           0 != count_query.value(0).toInt())
        {
            musicdata_exists = true;
        }
    }

    //  Load all available info about songs (once!)
    QString startdir = gContext->GetSetting("MusicLocation");
    startdir = QDir::cleanPath(startdir);
    if (!startdir.endsWith("/"))
        startdir += "/";

    Metadata::SetStartdir(startdir);

    Decoder::SetLocationFormatUseTags();

    // Only search music files if a directory was specified & there
    // is no data in the database yet (first run).  Otherwise, user
    // can choose "Setup" option from the menu to force it.
    if (startdir != "" && !musicdata_exists)
    {
        FileScanner *fscan = new FileScanner();
        fscan->SearchDir(startdir);
    }

    QString paths = gContext->GetSetting("TreeLevels");

    // Set the various track formatting modes
    Metadata::setArtistAndTrackFormats();

    AllMusic *all_music = new AllMusic(paths, startdir);

    //  Load all playlists into RAM (once!)
    PlaylistContainer *all_playlists = new PlaylistContainer(
        all_music, gContext->GetHostName());

    gMusicData->paths = paths;
    gMusicData->startdir = startdir;
    gMusicData->all_playlists = all_playlists;
    gMusicData->all_music = all_music;
}

static void postMusic()
{
    // Automagically save all playlists and metadata (ratings) that have changed
    if (gMusicData->all_music->cleanOutThreads())
    {
        gMusicData->all_music->save();
    }

    if (gMusicData->all_playlists->cleanOutThreads())
    {
        gMusicData->all_playlists->save();
        int x = gMusicData->all_playlists->getPending();
        SavePending(x);
    }

    delete gMusicData->all_music;
    gMusicData->all_music = NULL;
    delete gMusicData->all_playlists;
    gMusicData->all_playlists = NULL;
}

int mythplugin_run(void)
{
    gMusicData->runPost = true;

    preMusic();
    runMenu("musicmenu.xml");

    return 0;
}

int mythplugin_config(void)
{
    gMusicData->runPost = false;
    gMusicData->paths = gContext->GetSetting("TreeLevels");
    gMusicData->startdir = gContext->GetSetting("MusicLocation");
    gMusicData->startdir = QDir::cleanPath(gMusicData->startdir);

    if (!gMusicData->startdir.endsWith("/"))
        gMusicData->startdir += "/";

    Metadata::SetStartdir(gMusicData->startdir);

    Decoder::SetLocationFormatUseTags();

    runMenu("music_settings.xml");

    return 0;
}

void mythplugin_destroy(void)
{
    gPlayer->deleteLater();
    delete gMusicData;
}

void runMusicPlayback(void)
{
    gContext->addCurrentLocation("playmusic");
    preMusic();
    startPlayback();
    postMusic();
    gContext->removeCurrentLocation();
}

void runMusicSelection(void)
{
    gContext->addCurrentLocation("musicplaylists");
    preMusic();
    startDatabaseTree();
    postMusic();
    gContext->removeCurrentLocation();
}

void runRipCD(void)
{
    preMusic();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    Ripper *rip = new Ripper(mainStack, chooseCD());

    if (rip->Create())
        mainStack->AddScreen(rip);
    else
        delete rip;

//   connect(rip, SIGNAL(Success()), SLOT(RebuildMusicTree()));

//     postMusic();
}

void runScan(void)
{
    preMusic();

    if ("" != gMusicData->startdir)
    {
        FileScanner *fscan = new FileScanner();
        fscan->SearchDir(gMusicData->startdir);
        RebuildMusicTree();
    }

    postMusic();
}

void showMiniPlayer(void)
{
    // only show the miniplayer if there isn't already a client attached
    if (!gPlayer->hasClient())
        gPlayer->showMiniPlayer();
}
