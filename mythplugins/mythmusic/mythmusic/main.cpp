#include <qdir.h>
#include <iostream>
using namespace std;

#include <qapplication.h>
#include <qsqldatabase.h>
#include <qregexp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cdaudio.h>

#include "decoder.h"
#include "metadata.h"
#include "maddecoder.h"
#include "vorbisdecoder.h"
#include "databasebox.h"
#include "playbackbox.h"
#include "cdrip.h"
#include "playlist.h"
#include "globalsettings.h"
#include "dbcheck.h"
#include "importmusic.h"
#include "filescanner.h"

#include <mythtv/mythcontext.h>
#include <mythtv/mythplugin.h>
#include <mythtv/mythmediamonitor.h>
#include <mythtv/mythdbcon.h>

#include <mythtv/libmythui/myththemedmenu.h>

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
    char filename[1024];
    if (getenv("HOME") == NULL)
    {
        VERBOSE(VB_IMPORTANT, "main.o: You don't have a HOME environment variable. CD lookup will almost certainly not work.");
        return;
    }
    sprintf(filename, "%s/.cdserverrc", getenv("HOME"));

    QFile file(filename);

    if (!file.exists())
    {
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

void startPlayback(PlaylistsContainer *all_playlists, AllMusic *all_music)
{
    PlaybackBoxMusic *pbb;
    pbb = new PlaybackBoxMusic(gContext->GetMainWindow(),
                               "music_play", "music-", all_playlists,
                               all_music, chooseCD(), "music_playback");
    qApp->unlock();
    pbb->exec();
    qApp->lock();

    pbb->stop();

    qApp->processEvents();

    delete pbb;
}

void startDatabaseTree(PlaylistsContainer *all_playlists, AllMusic *all_music)
{
    DatabaseBox dbbox(all_playlists, all_music, gContext->GetMainWindow(),
                      chooseCD(), "music_select", "music-", "music database");
    qApp->unlock();
    dbbox.exec();
    qApp->lock();
}

bool startRipper(void)
{
    Ripper rip(chooseCD(), gContext->GetMainWindow(), "cd ripper");

    qApp->unlock();
    rip.exec();
    qApp->lock();

    if (rip.somethingWasRipped())
      return true;

    return false;
}

bool startImport(void)
{
    ImportMusicDialog import(gContext->GetMainWindow(), "import music");

    qApp->unlock();
    import.exec();
    qApp->lock();

    if (import.somethingWasImported())
        return true;

    return false;
}

struct MusicData
{
    QString paths;
    QString startdir;
    PlaylistsContainer *all_playlists;
    AllMusic *all_music;
    bool runPost;
};

void RebuildMusicTree(MusicData *mdata)
{
    MythBusyDialog busy(QObject::tr("Rebuilding music tree"));
    busy.start();
    mdata->all_music->startLoading();
    while (!mdata->all_music->doneLoading())
    {
        qApp->processEvents();
        usleep(50000);
    }
    mdata->all_playlists->postLoad();
    busy.Close();
}

static void postMusic(MusicData *mdata);

void MusicCallback(void *data, QString &selection)
{
    MusicData *mdata = (MusicData *)data;

    QString sel = selection.lower();

    if (sel == "music_create_playlist")
        startDatabaseTree(mdata->all_playlists, mdata->all_music);
    else if (sel == "music_play")
        startPlayback(mdata->all_playlists, mdata->all_music);
    else if (sel == "music_rip")
    {
        if (startRipper())
        {
            // If startRipper returns true, then new data should be present

            //  Tell the metadata to reset itself
            RebuildMusicTree(mdata);
        }
    }
    else if (sel == "music_import")
    {
        if (startImport())
            RebuildMusicTree(mdata);
    }
    else if (sel == "settings_scan")
    {
        if ("" != mdata->startdir)
        {
            FileScanner *fscan = new FileScanner();
            fscan->SearchDir(mdata->startdir);
            RebuildMusicTree(mdata);
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
        if (mdata)
        {
            if (mdata->runPost)
                postMusic(mdata);
            delete mdata;
        }
    }
}

void runMenu(MusicData *mdata, QString which_menu)
{
    QString themedir = gContext->GetThemeDir();

    MythThemedMenu *diag = new MythThemedMenu(themedir.ascii(), which_menu,
                                              GetMythMainWindow()->GetMainStack(),
                                              "music menu");

    diag->setCallback(MusicCallback, mdata);
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
        VERBOSE(VB_IMPORTANT, QString("Couldn't find theme %1").arg(themedir));
        delete diag;
    }
}

extern "C" {
int mythplugin_init(const char *libversion);
int mythplugin_run(void);
int mythplugin_config(void);
}

void runMusicPlayback(void);
void runMusicSelection(void);
void runRipCD(void);
void runScan(void);


void handleMedia(MythMediaDevice *cd)
{
    // Note that we should deal with other disks that may contain music.
    // e.g. MEDIATYPE_MMUSIC or MEDIATYPE_MIXED

    if (cd) 
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

    REG_KEY("Music", "DELETE",     "Delete track from playlist", "D");
    REG_KEY("Music", "NEXTTRACK",  "Move to the next track",     ">,.,Z,End");
    REG_KEY("Music", "PREVTRACK",  "Move to the previous track", ",,<,Q,Home");
    REG_KEY("Music", "FFWD",       "Fast forward",               "PgDown");
    REG_KEY("Music", "RWND",       "Rewind",                     "PgUp");
    REG_KEY("Music", "PAUSE",      "Pause/Start playback",       "P");
    REG_KEY("Music", "PLAY",       "Re/Start playback",          "");
    REG_KEY("Music", "STOP",       "Stop playback",              "O");
    REG_KEY("Music", "VOLUMEDOWN", "Volume down",                "[,{,F10");
    REG_KEY("Music", "VOLUMEUP",   "Volume up",                  "],},F11");
    REG_KEY("Music", "MUTE",       "Mute",                       "|,\\,F9");
    REG_KEY("Music", "CYCLEVIS",   "Cycle visualizer mode",      "6");
    REG_KEY("Music", "BLANKSCR",   "Blank screen",               "5");
    REG_KEY("Music", "THMBUP",     "Increase rating",            "9");
    REG_KEY("Music", "THMBDOWN",   "Decrease rating",            "7");
    REG_KEY("Music", "REFRESH",    "Refresh music tree",         "8");
    REG_KEY("Music", "FILTER",     "Filter All My Music",        "F");
    REG_KEY("Music", "INCSEARCH",     "Show incremental search dialog",     "Ctrl+S");
    REG_KEY("Music", "INCSEARCHNEXT", "Incremental search find next match", "Ctrl+N");

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
    general.load();
    general.save();

    MusicPlayerSettings settings;
    settings.load();
    settings.save();

    MusicRipperSettings ripper;
    ripper.load();
    ripper.save();

    setupKeys();

    Decoder::SetLocationFormatUseTags();

    return 0;
}

static void preMusic(MusicData *mdata)
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
    startdir = QDir::cleanDirPath(startdir);
    if (!startdir.endsWith("/"));
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
    PlaylistsContainer *all_playlists = new PlaylistsContainer(all_music, gContext->GetHostName());

    mdata->paths = paths;
    mdata->startdir = startdir;
    mdata->all_playlists = all_playlists;
    mdata->all_music = all_music;
}

static void postMusic(MusicData *mdata)
{
    // Automagically save all playlists and metadata (ratings) that have changed
    if (mdata->all_music->cleanOutThreads())
    {
        mdata->all_music->save();
    }

    if (mdata->all_playlists->cleanOutThreads())
    {
        mdata->all_playlists->save();
        int x = mdata->all_playlists->getPending();
        SavePending(x);
    }

    delete mdata->all_music;
    delete mdata->all_playlists;
}

int mythplugin_run(void)
{
    MusicData *mdata = new MusicData();
    mdata->runPost = true;

    preMusic(mdata);
    runMenu(mdata, "musicmenu.xml");

    return 0;
}

int mythplugin_config(void)
{
    MusicData *mdata = new MusicData();
    mdata->runPost = false;
    mdata->paths = gContext->GetSetting("TreeLevels");
    mdata->startdir = gContext->GetSetting("MusicLocation");
    mdata->startdir = QDir::cleanDirPath(mdata->startdir);
    
    if (!mdata->startdir.endsWith("/"))
        mdata->startdir += "/";

    Metadata::SetStartdir(mdata->startdir);

    Decoder::SetLocationFormatUseTags();

    runMenu(mdata, "music_settings.xml");

    return 0;
}

void runMusicPlayback(void)
{
    MusicData mdata;

    gContext->addCurrentLocation("playmusic");
    preMusic(&mdata);
    startPlayback(mdata.all_playlists, mdata.all_music);
    postMusic(&mdata);
    gContext->removeCurrentLocation();
}

void runMusicSelection(void)
{
    MusicData mdata;

    gContext->addCurrentLocation("musicplaylists");
    preMusic(&mdata);
    startDatabaseTree(mdata.all_playlists, mdata.all_music);
    postMusic(&mdata);
    gContext->removeCurrentLocation();
}

void runRipCD(void)
{
    MusicData mdata;

    gContext->addCurrentLocation("ripcd");
    preMusic(&mdata);
    if (startRipper())
    {
        // if startRipper returns true, then new files should be present
        // so we should look for them.
        FileScanner *fscan = new FileScanner();
        fscan->SearchDir(mdata.startdir);
        RebuildMusicTree(&mdata);
    }
    postMusic(&mdata);
    gContext->removeCurrentLocation();
}

void runScan(void)
{
    MusicData mdata;

    preMusic(&mdata);

    if ("" != mdata.startdir)
    {
        FileScanner *fscan = new FileScanner();
        fscan->SearchDir(mdata.startdir);
        RebuildMusicTree(&mdata);
    }

    postMusic(&mdata);
}
