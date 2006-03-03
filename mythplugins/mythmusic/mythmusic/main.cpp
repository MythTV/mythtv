#include <qdir.h>
#include <iostream>
#include <map>
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

#include <mythtv/mythcontext.h>
#include <mythtv/mythplugin.h>
#include <mythtv/mythmedia.h>
#include <mythtv/mythdbcon.h>

#include <mythtv/libmythui/myththemedmenu.h>

void CheckFreeDBServerFile(void)
{
    char filename[1024];
    if (getenv("HOME") == NULL)
    {
        cerr << "main.o: You don't have a HOME environment variable. CD lookup will almost certainly not work." << endl;
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

Decoder *getDecoder(const QString &filename)
{
    Decoder *decoder = Decoder::create(filename, NULL, NULL, true);
    return decoder;
}

void AddFileToDB(const QString &filename)
{
    Decoder *decoder = getDecoder(filename);

    if (decoder)
    {
        Metadata *data = decoder->getMetadata();
        if (data) {
            data->dumpToDatabase();
            delete data;
        }

        delete decoder;
    }
}

// Remove a file from the database
void RemoveFileFromDB (const QString &directory, const QString &filename)
{
    QString name(filename);
    name.remove(0, directory.length());
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM musicmetadata WHERE "
                  "filename = :NAME ;");
    query.bindValue(":NAME", filename.utf8());
    query.exec();
}

void UpdateFileInDB(const QString &filename)
{
    Decoder *decoder = getDecoder(filename);

    if (decoder)
    {
        Metadata *db_meta   = decoder->getMetadata();
        Metadata *disk_meta = decoder->readMetadata();

        if (db_meta && disk_meta)
        {
            disk_meta->setID(db_meta->ID());
            disk_meta->setRating(db_meta->Rating());
            disk_meta->updateDatabase();
        }

        if (disk_meta)
            delete disk_meta;

        if (db_meta)
            delete db_meta;

        delete decoder;
    }
}

enum MusicFileLocation
{
    kFileSystem,
    kDatabase,
    kNeedUpdate,
    kBoth
};

typedef QMap <QString, MusicFileLocation> MusicLoadedMap;

void BuildFileList(QString &directory, MusicLoadedMap &music_files)
{
    QDir d(directory);

    if (!d.exists())
        return;

    const QFileInfoList *list = d.entryInfoList();
    if (!list)
        return;

    QFileInfoListIterator it(*list);
    QFileInfo *fi;

    /* Recursively traverse directory, calling QApplication::processEvents()
       every now and then to ensure the UI updates */
    int update_interval = 0;
    while ((fi = it.current()) != 0)
    {
        ++it;
        if (fi->fileName() == "." || fi->fileName() == "..")
            continue;
        QString filename = fi->absFilePath();
        if (fi->isDir())
        {
            BuildFileList(filename, music_files);
            qApp->processEvents ();
        }
        else
        {
            if (++update_interval > 100)
            {
                qApp->processEvents();
                update_interval = 0;
            }
            music_files[filename] = kFileSystem;
        }
    }
}

bool HasFileChanged(const QString &filename, const QString &date_modified)
{
    struct stat sbuf;
    if (stat(filename.ascii(), &sbuf) == 0)
    {
        if (date_modified.isEmpty() ||
            sbuf.st_mtime >
            (time_t)QDateTime::fromString(date_modified,
                                          Qt::ISODate).toTime_t())
        {
            return true;
        }
    }
    return false;
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

void SearchDir(QString &directory)
{
    MusicLoadedMap music_files;
    MusicLoadedMap::Iterator iter;

    MythBusyDialog *busy = new MythBusyDialog(
        QObject::tr("Searching for music files"));

    busy->start();
    BuildFileList(directory, music_files);
    busy->Close();
    delete busy;

    MSqlQuery query(MSqlQuery::InitCon());
    query.exec("SELECT filename, date_modified "
               "FROM musicmetadata "
               "WHERE filename NOT LIKE ('%://%')");

    int counter = 0;

    MythProgressDialog *file_checking;
    file_checking = new MythProgressDialog(
        QObject::tr("Scanning music files"), query.numRowsAffected());

    if (query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            QString name = directory +
                QString::fromUtf8(query.value(0).toString());

            if (name != QString::null)
            {
                if ((iter = music_files.find(name)) != music_files.end())
                {
                    if (HasFileChanged(name, query.value(1).toString()))
                        music_files[name] = kNeedUpdate;
                    else
                        music_files.remove(iter);
                }
                else
                {
                    music_files[name] = kDatabase;
                }
            }
            file_checking->setProgress(++counter);
        }
    }

    file_checking->Close();
    delete file_checking;

    file_checking = new MythProgressDialog(
        QObject::tr("Updating music database"), music_files.size());

     /*
       This can be optimised quite a bit by consolidating all commands
       via a lot of refactoring.
       
       1) group all files of the same decoder type, and don't
       create/delete a Decoder pr. AddFileToDB. Or make Decoders be
       singletons, it should be a fairly simple change.
       
       2) RemoveFileFromDB should group the remove into one big SQL.
       
       3) UpdateFileInDB, same as 1.
     */

    counter = 0;
    for (iter = music_files.begin(); iter != music_files.end(); iter++)
    {
        if (*iter == kFileSystem)
            AddFileToDB(iter.key());
        else if (*iter == kDatabase)
            RemoveFileFromDB(directory, iter.key ());
        else if (*iter == kNeedUpdate)
            UpdateFileInDB(iter.key());

        file_checking->setProgress(++counter);
    }
    file_checking->Close();
    delete file_checking;
}

void startPlayback(PlaylistsContainer *all_playlists, AllMusic *all_music)
{
    PlaybackBoxMusic *pbb = new PlaybackBoxMusic(gContext->GetMainWindow(),
                                                 "music_play", "music-", 
                                                 all_playlists, all_music,
                                                 "music_playback");
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
                      "music_select", "music-", "music database");
    qApp->unlock();
    dbbox.exec();
    qApp->lock();
}

bool startRipper(void)
{
    Ripper rip(gContext->GetMainWindow(), "cd ripper");

    qApp->unlock();
    rip.exec();
    qApp->lock();

    if (rip.somethingWasRipped())
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

            //  Reconcile with the database
            SearchDir(mdata->startdir);
            //  Tell the metadata to reset itself
            RebuildMusicTree(mdata);
        }
    }
    else if (sel == "settings_scan")
    {
        if ("" != mdata->startdir)
        {
            SearchDir(mdata->startdir);
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
        if (mdata->runPost)
            postMusic(mdata);
        delete mdata;
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
        cerr << "Couldn't find theme " << themedir << endl;
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


void handleMedia(MythMediaDevice *) 
{
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

    REG_KEY("Music", "DELETE",     "Delete track from playlist", "D");
    REG_KEY("Music", "NEXTTRACK",  "Move to the next track",     ">,.,Z,End");
    REG_KEY("Music", "PREVTRACK",  "Move to the previous track", ",,<,Q,Home");
    REG_KEY("Music", "FFWD",       "Fast forward",               "PgDown");
    REG_KEY("Music", "RWND",       "Rewind",                     "PgUp");
    REG_KEY("Music", "PAUSE",      "Pause/Start playback",       "P");
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

    REG_MEDIA_HANDLER("MythMusic Media Handler", "", "", handleMedia,
                      MEDIATYPE_AUDIO | MEDIATYPE_MIXED);
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythmusic", libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    gContext->ActivateSettingsCache(false);
    UpgradeMusicDatabaseSchema();
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
    count_query.exec("SELECT COUNT(*) FROM musicmetadata;");

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
        SearchDir(startdir);

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
    //postMusic(&mdata);

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
        SearchDir(mdata.startdir);
        RebuildMusicTree(&mdata);
    }
    postMusic(&mdata);
    gContext->removeCurrentLocation();
}
